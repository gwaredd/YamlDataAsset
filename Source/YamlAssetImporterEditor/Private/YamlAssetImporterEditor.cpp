#include "YamlAssetImporterEditor.h"

DEFINE_LOG_CATEGORY( LogYamlAssetImporter );

#define LOCTEXT_NAMESPACE "FYamlAssetImporterEditor"

void FYamlAssetImporterEditor::StartupModule()
{
}

void FYamlAssetImporterEditor::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE( FYamlAssetImporterEditor, YamlAssetImporter )
