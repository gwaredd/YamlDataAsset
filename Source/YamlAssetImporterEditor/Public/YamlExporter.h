// Copyright (C) 2024 Gwaredd Mountain - All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "Exporters/Exporter.h"
#include "YamlExporter.generated.h"

class FExportObjectInnerContext;

UCLASS()
class UYamlExporter : public UExporter
{
    GENERATED_UCLASS_BODY()

public:
    virtual bool ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, uint32 PortFlags = 0 ) override;
};

