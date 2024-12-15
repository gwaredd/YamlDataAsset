// Copyright (C) 2024 Gwaredd Mountain - All Rights Reserved.

#include "YamlDataAssetEditorModule.h"

DEFINE_LOG_CATEGORY( LogYamlDataAsset );

#define LOCTEXT_NAMESPACE "FYamlAssetImporterEditor"

void FYamlDataAssetEditorModule::StartupModule()
{
}

void FYamlDataAssetEditorModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE( FYamlDataAssetEditorModule, YamlDataAsset )
