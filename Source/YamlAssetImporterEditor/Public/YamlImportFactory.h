// Copyright (C) 2024 Gwaredd Mountain - All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "CoreMinimal.h"
#include "YamlImportFactory.generated.h"

UCLASS( hidecategories = Object )
class UYamlImportFactory : public UFactory
{
    GENERATED_UCLASS_BODY()

public:

    virtual UObject* FactoryCreateFile( UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled ) override;


protected:

    bool SelectClass( bool& bOutOperationCanceled );

    TSharedPtr<SWindow> Modal;
    TArray<UClass*>     Classes;
    UClass*             SelectedClass = nullptr;
};
