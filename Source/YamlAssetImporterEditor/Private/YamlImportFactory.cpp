// Copyright (C) 2024 Gwaredd Mountain - All Rights Reserved.

#include "YamlImportFactory.h"
#include "YamlAssetImporterEditor.h"
#include "Engine/DataAsset.h"
#include "Interfaces/IMainFrameModule.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "YamlImportFactory"

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

UObject* UYamlImportFactory::FactoryCreateFile( UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled )
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
        Selected = Classes[ 0 ];
    }

    // select asset

    Modal = SNew( SWindow )
    .Title( LOCTEXT( "YamlAssetWindowTitle", "Select Custom Asset" ) )
    .SizingRule( ESizingRule::Autosized )
    [
        SNew( SBorder )
        .BorderImage( FAppStyle::GetBrush("Menu.Background") )
        [
            SNew( SVerticalBox )

            + SVerticalBox::Slot()
            .Padding( 4 )
            [
                SNew( SComboBox<UClass*> )
                .OptionsSource( &Classes )
                .OnGenerateWidget_Lambda( [this]( const UClass* In ) { return SNew( STextBlock ).Text( FText::FromName( In->GetFName() ) ); } )
                .OnSelectionChanged_Lambda( [this]( UClass* In , ESelectInfo::Type Type ) { Selected = In; } )
                [
                    SNew( STextBlock )
                        .Text_Lambda( [this]() { return FText::FromName( Selected->GetFName() ); } )
                ]
            ]

            + SVerticalBox::Slot()
            .Padding( 4 )
            .AutoHeight()
            [
                SNew( SHorizontalBox )

                // cancel
                + SHorizontalBox::Slot()
                .FillWidth(0.5f)
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
                .FillWidth(0.5f)
                .Padding( 4 )
                [
                    SNew( SButton )
                        .ButtonStyle( FAppStyle::Get(), "Button" )
                        .TextStyle( FAppStyle::Get(), "DialogButtonText" )
                        .HAlign( HAlign_Center )
                        .VAlign( VAlign_Center )
                        .Text( LOCTEXT( "OkButtonLabel", "Ok" ) )
                        .OnClicked_Lambda( [this]() { Modal->RequestDestroyWindow(); return FReply::Handled(); })
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

    if( !Selected )
    {
        bOutOperationCanceled = true;
    }

    if( bOutOperationCanceled )
    {
        return nullptr;
    }


    // load file

    FString TextString;

    if( !FFileHelper::LoadFileToString( TextString, *Filename ) )
    {
        UE_LOG( LogYamlAssetImporter, Error, TEXT( "Failed to load %s" ), *Filename );
        return nullptr;
    }

    // create asset

    auto Asset = NewObject<UDataAsset>( InParent, Selected, InName, Flags);

    if( !Asset )
    {
        UE_LOG( LogYamlAssetImporter, Error, TEXT( "Failed to create %s asset" ), *Selected->GetFName().ToString() );
        return nullptr;
    }

    // parse yaml

    return Asset;
}

#undef LOCTEXT_NAMESPACE
