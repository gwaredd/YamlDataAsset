// Copyright (C) 2024 Gwaredd Mountain - All Rights Reserved.

#include "YamlImportFactory.h"
#include "YamlAssetImporterEditor.h"
#include "Engine/DataAsset.h"
#include "Interfaces/IMainFrameModule.h"
#include "Editor.h"
#include "yaml-cpp/include/yaml.h"

#define LOCTEXT_NAMESPACE "YamlImportFactory"


//-------------------------------------------------------------------------------------------------
// helper functions

static constexpr uint64 ScalarTypes =
    CASTCLASS_FBoolProperty |
    CASTCLASS_FEnumProperty |
    CASTCLASS_FNumericProperty |
    CASTCLASS_FNameProperty |
    CASTCLASS_FStrProperty |
    CASTCLASS_FTextProperty |
    CASTCLASS_FLargeWorldCoordinatesRealProperty |
    CASTCLASS_FClassProperty |
    CASTCLASS_FObjectProperty |
    CASTCLASS_FWeakObjectProperty |
    CASTCLASS_FLazyObjectProperty |
    CASTCLASS_FSoftObjectProperty |
    CASTCLASS_FSoftClassProperty;

// map YAML::NodeType to supported FProperty types

static uint64 GetSupportedPropertyTypeFlags( YAML::NodeType::value Type )
{
    switch( Type )
    {
        case YAML::NodeType::Undefined:
        case YAML::NodeType::Null:      return CASTCLASS_AllFlags;
        case YAML::NodeType::Scalar:    return ScalarTypes;
        case YAML::NodeType::Sequence:  return CASTCLASS_FArrayProperty | CASTCLASS_FSetProperty;
        case YAML::NodeType::Map:       return CASTCLASS_FStructProperty | CASTCLASS_FMapProperty;
        default:                        return 0;
    }
}

// YAML::NodeType to string for error logging

static const char* GetNodeTypeName( YAML::NodeType::value Type )
{
    switch( Type )
    {
        default:                        return "Unknown";
        case YAML::NodeType::Undefined: return "Undefined";
        case YAML::NodeType::Null:      return "Null";
        case YAML::NodeType::Scalar:    return "Scalar";
        case YAML::NodeType::Sequence:  return "Sequence";
        case YAML::NodeType::Map:       return "Map";
    }
}


//-------------------------------------------------------------------------------------------------
// Set the value of a given property from the Yaml
// 
// Address : memory address of the value (this is "direct", we need to resolve the property address from the container before calling)
// Property: property reflection data
// Node    : yaml node to use to populate the value
//

static bool SetProperty( void* Address, FProperty* Property, YAML::Node Node )
{
    auto NodeType = Node.Type();

    // check we can convert the YAML::Node type to the given FProperty

    if( ( Property->GetCastFlags() & GetSupportedPropertyTypeFlags( NodeType ) ) == 0 )
    {
        UE_LOG( LogYamlAssetImporter, Warning, TEXT( "Property: %s - can't convert from yaml:%hs to %s" ),
            *Property->GetFName().ToString(),
            GetNodeTypeName( Node.Type() ),
            *Property->GetClass()->GetFName().ToString()
        );

        return false;
    }

    // set value from yaml

    switch( NodeType )
    {
        // NodeType::Null -> clear property

        case YAML::NodeType::Undefined:
        case YAML::NodeType::Null:
        {
            Property->ClearValue( Address );
        }
        break;


        // NodeType::Scalar -> FProperty value type

        case YAML::NodeType::Scalar:
        {
            auto Value = StringCast<TCHAR>( Node.as<std::string>().c_str() ).Get();
            Property->ImportText_Direct( Value, Address, nullptr, PPF_None );
        }
        break;


        // NodeType::Sequence[] -> TArray or a TSet

        case YAML::NodeType::Sequence:
        {
            // FArrayProperty

            if( auto ArrayField = CastField<FArrayProperty>( Property ) )
            {
                // create empty array of required size

                FScriptArrayHelper ArrayHelper( ArrayField, Address );
                ArrayHelper.Resize( Node.size() );

                // set value of each element

                for( std::size_t Index = 0; Index < Node.size(); ++Index )
                {
                    SetProperty( ArrayHelper.GetElementPtr( Index ), ArrayField->Inner, Node[ Index ] );
                }
            }

            // FSetProperty

            else if( auto SetField = CastField<FSetProperty>( Property ) )
            {
                // create empty set

                FScriptSetHelper SetHelper( SetField, Address );
                SetHelper.EmptyElements();

                // BUG: this can create duplicate items and therefore is an invalidate set
                // solution may be to create a dummy entry, populate that and use that as a way to
                // check existence, then remove it at the end. Bit pants but should theorectically work!

                // add items

                for( std::size_t Index = 0; Index < Node.size(); ++Index )
                {
                    auto ElementIndex = SetHelper.AddDefaultValue_Invalid_NeedsRehash();
                    auto ElementAddr  = SetHelper.GetElementPtr( ElementIndex );

                    SetProperty( ElementAddr, SetField->ElementProp, Node[ Index ] );
                }

                SetHelper.Rehash();
            }
        }
        break;


        // NodeType::Map{} -> UStruct or TMap

        case YAML::NodeType::Map:
        {
            // FStructProperty

            if( auto StructProperty = CastField<FStructProperty>( Property ) )
            {
                auto StructClass = StructProperty->Struct;

                for( const auto& Child : Node )
                {
                    auto Key = FName ( Child.first.as<std::string>().c_str() );

                    if( auto FieldProperty = StructClass->FindPropertyByName( Key ) )
                    {
                        auto FieldAddress = FieldProperty->ContainerPtrToValuePtr<uint8>( Address );
                        SetProperty( FieldAddress, FieldProperty, Child.second );
                    }
                    else
                    {
                        UE_LOG( LogYamlAssetImporter, Warning, TEXT( "Failed to find property %s in %s" ), *Key.ToString(), *StructClass->GetFName().ToString() );
                    }
                }
            }

            // FMapProperty

            else if( auto MapProperty = CastField<FMapProperty>( Property ) )
            {
                FScriptMapHelper MapHelper( MapProperty, Address );
                MapHelper.EmptyValues();

                for( const auto& Child : Node )
                {
                    auto MapIndex = MapHelper.AddDefaultValue_Invalid_NeedsRehash();
                    SetProperty( MapHelper.GetKeyPtr( MapIndex ),   MapProperty->KeyProp,   Child.first  );
                    SetProperty( MapHelper.GetValuePtr( MapIndex ), MapProperty->ValueProp, Child.second );
                }

                MapHelper.Rehash();
            }
        }
        break;


        // unknown type - shouldn't never (happen unless yaml one day adds something new or there is some egregious memory trample)!

        default:
        {
            UE_LOG( LogYamlAssetImporter, Error, TEXT( "Unknown YAML node type!!!" ) );
            return false;
        }
    }

    return true;
}


//-------------------------------------------------------------------------------------------------
// fill in fields from given asset

static UObject* ProcessObject( UObject* Asset, YAML::Node Node )
{
    auto Class = Asset->GetClass();

    for( const auto& Child : Node )
    {
        auto Key = FName( Child.first.as<std::string>().c_str() );

        // ignore class specifier

        if( Key == FName( "__uclass" ) )
        {
            continue;
        }

        // look for named property in the asset

        auto Property = Class->FindPropertyByName( Key );

        if( !Property )
        {
            UE_LOG( LogYamlAssetImporter, Warning, TEXT( "Failed to find property %s in %s" ), *Key.ToString(), *Class->GetFName().ToString() );
            continue;
        }

        // set value

        auto Address = Property->ContainerPtrToValuePtr<uint8>( Asset );
        SetProperty( Address, Property, Child.second );
    }

    return Asset;
}

//-------------------------------------------------------------------------------------------------

UYamlImportFactory::UYamlImportFactory( const FObjectInitializer& ObjectInitializer )
    : Super( ObjectInitializer )
{
    Formats.Add( TEXT( "yaml;YAML file format for custom classes" ) );
    SupportedClass = UDataAsset::StaticClass();
    bCreateNew     = false;
    bEditorImport  = true;
}


//-------------------------------------------------------------------------------------------------
// get list of all registered UDataAsset's and select the given one if specified

void UYamlImportFactory::GetDataAssets( FName FindClass )
{
    SelectedClass = nullptr;

    // get list of UDataAsset's

    for( TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt )
    {
        UClass* Class = *ClassIt;

        if( !Class->IsNative() )
        {
            continue;
        }

        if( Class->HasAnyClassFlags( CLASS_Deprecated | CLASS_NewerVersionExists ) )
        {
            continue;
        }

        if( !Class->IsChildOf( UDataAsset::StaticClass() ) || Class == UDataAsset::StaticClass() )
        {
            continue;
        }

        if( Class->GetFName() == FindClass )
        {
            SelectedClass = Class;
        }

        Classes.Add( Class );
    }

    Classes.Sort( []( UClass& A, UClass& B ) { return A.GetFName().Compare( B.GetFName() ) < 0; } );
}
    

//-------------------------------------------------------------------------------------------------
// get user to select a UDataAsset to use from the ones registered

bool UYamlImportFactory::SelectClassModal( bool& bOutOperationCanceled )
{
    bOutOperationCanceled = false;

    // select custom asset

    Modal = SNew( SWindow )
        .Title( LOCTEXT( "YamlAssetWindowTitle", "Select Custom Asset" ) )
        .SizingRule( ESizingRule::Autosized )
        [
            SNew( SBorder )
                .BorderImage( FAppStyle::GetBrush( "Menu.Background" ) )
                [
                    SNew( SVerticalBox )

                        + SVerticalBox::Slot()
                        .Padding( 4 )
                        [
                            SNew( SComboBox<UClass*> )
                                .OptionsSource( &Classes )
                                .OnGenerateWidget_Lambda( [this]( const UClass* In ) { return SNew( STextBlock ).Text( FText::FromName( In->GetFName() ) ); } )
                                .OnSelectionChanged_Lambda( [this]( UClass* In, ESelectInfo::Type Type ) { SelectedClass = In; } )
                                [
                                    SNew( STextBlock )
                                        .Text_Lambda( [this]() { return FText::FromName( SelectedClass ? SelectedClass->GetFName() : FName() ); } )
                                ]
                        ]

                        + SVerticalBox::Slot()
                        .Padding( 4 )
                        .AutoHeight()
                        [
                            SNew( SHorizontalBox )

                                // cancel
                                + SHorizontalBox::Slot()
                                .FillWidth( 0.5f )
                                .Padding( 4 )
                                [
                                    SNew( SButton )
                                        .ButtonStyle( FAppStyle::Get(), "Button" )
                                        .TextStyle( FAppStyle::Get(), "DialogButtonText" )
                                        .HAlign( HAlign_Center )
                                        .VAlign( VAlign_Center )
                                        .Text( LOCTEXT( "CancelButtonLabel", "Cancel" ) )
                                        .OnClicked_Lambda( [this, &bOutOperationCanceled]() { bOutOperationCanceled = true; Modal->RequestDestroyWindow(); return FReply::Handled(); } )
                                ]

                                // ok
                                + SHorizontalBox::Slot()
                                .FillWidth( 0.5f )
                                .Padding( 4 )
                                [
                                    SNew( SButton )
                                        .ButtonStyle( FAppStyle::Get(), "Button" )
                                        .TextStyle( FAppStyle::Get(), "DialogButtonText" )
                                        .HAlign( HAlign_Center )
                                        .VAlign( VAlign_Center )
                                        .Text( LOCTEXT( "OkButtonLabel", "Ok" ) )
                                        .OnClicked_Lambda( [this]() { Modal->RequestDestroyWindow(); return FReply::Handled(); } )
                                ]
                        ]
                ]
        ];

    TSharedPtr<SWindow> ParentWindow;

    if( FModuleManager::Get().IsModuleLoaded( "MainFrame" ) )
    {
        IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>( "MainFrame" );
        ParentWindow = MainFrame.GetParentWindow();
    }

    FSlateApplication::Get().AddModalWindow( Modal.ToSharedRef(), ParentWindow, false );

    if( !SelectedClass )
    {
        bOutOperationCanceled = true;
    }

    return !bOutOperationCanceled;
}


//-------------------------------------------------------------------------------------------------

UObject* UYamlImportFactory::FactoryCreateFile( UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled )
{
    // load file

    FString FileContents;

    if( !FFileHelper::LoadFileToString( FileContents, *Filename ) )
    {
        UE_LOG( LogYamlAssetImporter, Error, TEXT( "Failed to load %s" ), *Filename );
        return nullptr;
    }

    // parse YAML

    YAML::Node Doc;

    try
    {
        auto Buffer = StringCast<UTF8CHAR>( *FileContents, FileContents.Len() ); // yamp-cpp requires utf-8
        Doc = YAML::Load( (const char*) Buffer.Get() );
    }
    catch( ... )
    {
        UE_LOG( LogYamlAssetImporter, Error, TEXT( "Failed to parse %s" ), *Filename );
        return nullptr;
    }

    if( Doc.Type() != YAML::NodeType::Map )
    {
        UE_LOG( LogYamlAssetImporter, Error, TEXT( "Failed to load %s, expected object as the root" ), *Filename );
        return nullptr;
    }

    // get all registered UDataAsset's and look for the one set in this file (if specifed)

    FName FindClass = Doc[ "__uclass" ] ? FName( Doc[ "__uclass" ].as<std::string>().c_str() ) : FName();
    GetDataAssets( FindClass );

    // if we didn't find the __uclass (or none specified) then get the user to choose one

    if( !SelectedClass && !SelectClassModal( bOutOperationCanceled ) )
    {
        return nullptr;
    }

    // create the asset

    auto Asset = NewObject<UDataAsset>( InParent, SelectedClass, InName, Flags);

    if( !Asset )
    {
        UE_LOG( LogYamlAssetImporter, Error, TEXT( "Failed to create %s asset" ), *SelectedClass->GetFName().ToString() );
        return nullptr;
    }

    // fill in the fields from yaml

    return ProcessObject( Asset, Doc );
}


//-------------------------------------------------------------------------------------------------
#undef LOCTEXT_NAMESPACE
