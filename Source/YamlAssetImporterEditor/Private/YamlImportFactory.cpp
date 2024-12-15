// Copyright (C) 2024 Gwaredd Mountain - All Rights Reserved.

#include "YamlImportFactory.h"
#include "YamlAssetImporterEditor.h"
#include "Engine/DataAsset.h"
#include "Interfaces/IMainFrameModule.h"
#include "Editor.h"
#include "yaml-cpp/include/yaml.h"

#define LOCTEXT_NAMESPACE "YamlImportFactory"

//-------------------------------------------------------------------------------------------------

// all these can be set by text

static constexpr uint64 ScalarFlags =
    CASTCLASS_FBoolProperty |
    CASTCLASS_FByteProperty |
    CASTCLASS_FDoubleProperty |
    CASTCLASS_FEnumProperty |
    CASTCLASS_FFloatProperty |
    CASTCLASS_FInt16Property |
    CASTCLASS_FInt64Property |
    CASTCLASS_FInt8Property |
    CASTCLASS_FIntProperty |
    CASTCLASS_FNameProperty |
    CASTCLASS_FNumericProperty |
    CASTCLASS_FStrProperty |
    CASTCLASS_FTextProperty |
    CASTCLASS_FUInt16Property |
    CASTCLASS_FUInt32Property |
    CASTCLASS_FUInt64Property |
    CASTCLASS_FLargeWorldCoordinatesRealProperty |
    CASTCLASS_FClassProperty |
    CASTCLASS_FObjectProperty |
    CASTCLASS_FWeakObjectProperty |
    CASTCLASS_FLazyObjectProperty |
    CASTCLASS_FSoftObjectProperty |
    CASTCLASS_FSoftClassProperty;

static uint64 GetSupportedPropertyTypeFlags( YAML::NodeType::value Type )
{
    switch( Type )
    {
        default:                        return 0;
        case YAML::NodeType::Undefined: return ScalarFlags | CASTCLASS_FArrayProperty | CASTCLASS_FMapProperty | CASTCLASS_FSetProperty;
        case YAML::NodeType::Null:      return ScalarFlags | CASTCLASS_FArrayProperty | CASTCLASS_FMapProperty | CASTCLASS_FSetProperty;
        case YAML::NodeType::Scalar:    return ScalarFlags;
        case YAML::NodeType::Sequence:  return CASTCLASS_FArrayProperty | CASTCLASS_FSetProperty;
        case YAML::NodeType::Map:       return CASTCLASS_FStructProperty | CASTCLASS_FMapProperty;
    }
}

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

static bool SetProperty( void* Container, FProperty* Property, YAML::Node Node )
{
    auto NodeType = Node.Type();

    // check we can convert it

    if( ( Property->GetCastFlags() & GetSupportedPropertyTypeFlags( NodeType ) ) == 0 )
    {
        UE_LOG( LogYamlAssetImporter, Warning, TEXT( "Property: %s - can't convert from yaml:%hs to %s" ),
            *Property->GetFName().ToString(),
            GetNodeTypeName( Node.Type() ),
            *Property->GetClass()->GetFName().ToString()
        );

        return false;
    }

    // convert

    switch( NodeType )
    {
        // clear property

        case YAML::NodeType::Undefined:
        case YAML::NodeType::Null:
        {
            auto Memory = Property->ContainerPtrToValuePtr<uint8>( Container );

            if( auto ArrayProperty = CastField<FArrayProperty>( Property ) )
            {
                FScriptArrayHelper( ArrayProperty, Memory ).EmptyValues();
            }
            else if( auto MapProperty = CastField<FMapProperty>( Property ) )
            {
                FScriptMapHelper( MapProperty, Memory ).EmptyValues();
            }
            else if( auto SetProperty = CastField<FSetProperty>( Property ) )
            {
                FScriptSetHelper( SetProperty, Memory ).EmptyElements();
            }
            else
            {
                Property->ClearValue_InContainer( Container );
            }
        }
        break;


        // set property by string

        case YAML::NodeType::Scalar:
        {
            auto Value = Node.as<std::string>();
            Property->ImportText_InContainer( StringCast<TCHAR>( Value.c_str() ).Get(), Container, nullptr, PPF_None );
        }
        break;

        // array types

        case YAML::NodeType::Sequence:
        {
            auto Memory = Property->ContainerPtrToValuePtr<uint8>( Container );

            if( auto ArrayProperty = CastField<FArrayProperty>( Property ) )
            {
                FScriptArrayHelper ArrayHelper( ArrayProperty, Memory );

                ArrayHelper.Resize( Node.size() );

                for( std::size_t Index = 0; Index < Node.size(); ++Index )
                {
                    auto ElementNode = Node[ Index ];
                    auto ElementMemory = ArrayHelper.GetElementPtr( Index );

                    SetProperty( ElementMemory, ArrayProperty->Inner, ElementNode );
                }
            }
            else if( auto SetPropertyPtr = CastField<FSetProperty>( Property ) )
            {
                //FScriptSetHelper SetHelper( SetPropertyPtr, Memory );
                //SetHelper.EmptyElements();

                //auto ElementProperty = SetHelper.GetElementProperty();

                //for( std::size_t Index = 0; Index < Node.size(); ++Index )
                //{
                //    auto ElementIndex = SetHelper.AddDefaultValue_Invalid_NeedsRehash();
                //    auto Addr = SetHelper.GetElementPtr( ElementIndex );
                //    //SetProperty( Addr, ElementProperty, Node[ Index ] );
                //}

                //SetHelper.Rehash();
            }
        }
        break;

        // map types

        case YAML::NodeType::Map:
        {
            if( auto StructProperty = CastField<FStructProperty>( Property ) )
            {
                auto StructClass = StructProperty->Struct;

                for( const auto& Child : Node )
                {
                    auto  Key = FName( Child.first.as<std::string>().c_str() );
                    auto  Field = StructClass->FindPropertyByName( Key );

                    if( !Field )
                    {
                        UE_LOG( LogYamlAssetImporter, Warning, TEXT( "Failed to find property %s in %s" ), *Key.ToString(), *StructClass->GetFName().ToString() );
                        continue;
                    }

                    SetProperty( Container, Field, Child.second );
                }
            }
            else if( auto MapProperty = CastField<FMapProperty>( Property ) )
            {
                auto Memory = Property->ContainerPtrToValuePtr<uint8>( Container );
                FScriptMapHelper MapHelper( MapProperty, Memory );

                auto KeyProperty   = MapHelper.GetKeyProperty();
                auto ValueProperty = MapHelper.GetValueProperty();

                MapHelper.EmptyValues();

                for( const auto& Child : Node )
                {
                    auto Index = MapHelper.AddDefaultValue_Invalid_NeedsRehash();
                    SetProperty( MapHelper.GetKeyPtr( Index ), KeyProperty, Child.first );
                    SetProperty( MapHelper.GetValuePtr( Index ), ValueProperty, Child.second );
                }

                MapHelper.Rehash();
            }
        }
        break;

        default:
        {
            return false;
        }
    }

    return true;
}

//-------------------------------------------------------------------------------------------------

static void ProcessObject( UObject* Object, YAML::Node Node )
{
    auto Class = Object->GetClass();

    for( const auto& Child : Node )
    {
        auto  Key       = FName( Child.first.as<std::string>().c_str() );
        auto  Property  = Class->FindPropertyByName( Key );

        if( !Property )
        {
            UE_LOG( LogYamlAssetImporter, Warning, TEXT( "Failed to find property %s in %s" ), *Key.ToString(), *Class->GetFName().ToString() );
            continue;
        }

        SetProperty( Object, Property, Child.second );
    }
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

bool UYamlImportFactory::SelectClass( bool& bOutOperationCanceled )
{
    //GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPreImport( this, InClass, InParent, InName, Type );

    bOutOperationCanceled = false;

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

        Classes.Add( Class );
    }

    if( Classes.Num() > 0 )
    {
        SelectedClass = Classes[ 0 ];
    }

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
                                        .Text_Lambda( [this]() { return FText::FromName( SelectedClass->GetFName() ); } )
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
    //GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPreImport( this, InClass, InParent, InName, Type );

    if( !SelectClass( bOutOperationCanceled ) )
    {
        return nullptr;
    }

    // load file

    FString FileContents;

    if( !FFileHelper::LoadFileToString( FileContents, *Filename ) )
    {
        UE_LOG( LogYamlAssetImporter, Error, TEXT( "Failed to load %s" ), *Filename );
        return nullptr;
    }

    YAML::Node Node;

    try
    {
        auto Buffer = StringCast<UTF8CHAR>( *FileContents, FileContents.Len() );
        Node = YAML::Load( (const char*) Buffer.Get() );
    }
    catch(...)
    {
        UE_LOG( LogYamlAssetImporter, Error, TEXT( "Failed to parse %s" ), *Filename );
        return nullptr;
    }

    if( Node.Type() != YAML::NodeType::Map )
    {
        UE_LOG( LogYamlAssetImporter, Error, TEXT( "Failed to load %s, expected object as the root" ), *Filename );
        return nullptr;
    }

    // create asset

    auto Asset = NewObject<UDataAsset>( InParent, SelectedClass, InName, Flags);

    if( !Asset )
    {
        UE_LOG( LogYamlAssetImporter, Error, TEXT( "Failed to create %s asset" ), *SelectedClass->GetFName().ToString() );
        return nullptr;
    }

    ProcessObject( Asset, Node );

    return Asset;
}


//-------------------------------------------------------------------------------------------------
#undef LOCTEXT_NAMESPACE
