// Copyright (C) 2024 Gwaredd Mountain - All Rights Reserved.

#include "YamlExporter.h"
#include "yaml-cpp/include/yaml.h"


//-------------------------------------------------------------------------------------------------
// helper functions

std::string YamlStr( const FString& Str )
{
    return Str.IsEmpty() ? "" : reinterpret_cast<const char*>( StringCast<UTF8CHAR>(Str.GetCharArray().GetData()).Get() );
}

std::string YamlStr( const FName Name )
{
    return YamlStr( Name.ToString() );
}


//-------------------------------------------------------------------------------------------------

UYamlExporter::UYamlExporter( const FObjectInitializer& ObjectInitializer )
    : Super( ObjectInitializer )
{
    SupportedClass       = UDataAsset::StaticClass();
    bText                = true;
    PreferredFormatIndex = 0;

    FormatExtension.Add( TEXT( "yaml" ) );
    FormatDescription.Add( TEXT( "UDataAsset as yaml" ) );
}

//-------------------------------------------------------------------------------------------------

void ReflectProperty( void* Address, FProperty* Property, YAML::Emitter& out )
{
    if( auto Array = CastField<FArrayProperty>( Property ) )
    {
        out << YAML::BeginSeq;

        FScriptArrayHelper ArrayHelper( Array, Address );

        for( auto Index = 0; Index < ArrayHelper.Num(); ++Index )
        {
            ReflectProperty( ArrayHelper.GetElementPtr( Index ), Array->Inner, out );
        }

        out << YAML::EndSeq;
    }
    else if( auto Set = CastField<FSetProperty>( Property ) )
    {
        out << YAML::BeginSeq;

        FScriptSetHelper SetHelper( Set, Address );

        for( auto Index = 0; Index < SetHelper.Num(); ++Index )
        {
            ReflectProperty( SetHelper.GetElementPtr( Index ), Set->ElementProp, out );
        }

        out << YAML::EndSeq;
    }
    else if( auto Map = CastField<FMapProperty>( Property ) )
    {
        out << YAML::BeginMap;

        FString Value;
        FScriptMapHelper MapHelper( Map, Address );

        for( auto Index = 0; Index < MapHelper.Num(); ++Index )
        {
            Value.Reset();
            Map->KeyProp->ExportTextItem_Direct( Value, MapHelper.GetKeyPtr( Index ), nullptr, nullptr, PPF_None );
            out << YAML::Key << YamlStr( Value ) << YAML::Value;

            ReflectProperty( MapHelper.GetValuePtr( Index ), Map->ValueProp, out );
        }

        out << YAML::EndMap;
    }
    else if( auto Struct = CastField<FStructProperty>( Property ) )
    {
        out << YAML::BeginMap;

        for( TFieldIterator<FProperty> Field( Struct->Struct ); Field; ++Field )
        {
            out << YAML::Key << YamlStr( Field->GetFName() ) << YAML::Value;

            auto FieldAddress = Field->ContainerPtrToValuePtr<uint8>( Address );
            ReflectProperty( FieldAddress, *Field, out );
        }

        out << YAML::EndMap;
    }
    else
    {
        FString Value;
        Property->ExportTextItem_Direct( Value, Address, nullptr, nullptr, PPF_None );
        out << YAML::Value << YamlStr( Value );
    }
}


//-------------------------------------------------------------------------------------------------

bool UYamlExporter::ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, uint32 PortFlags )
{
    YAML::Emitter out;

    out << YAML::BeginMap;
    out << YAML::Key << "__uclass" << YAML::Value << YamlStr( Object->GetClass()->GetFName() );

    for( TFieldIterator<FProperty> Field( Object->GetClass() ); Field; ++Field )
    {
        auto Key = Field->GetFName();
        
        if( Key == FName( "NativeClass" ) )
        {
            continue;
        }

        out << YAML::Key << YamlStr( Key ) << YAML::Value;
        auto Address = Field->ContainerPtrToValuePtr<uint8>( Object );
        ReflectProperty( Address, *Field, out );
    }

    out << YAML::EndMap;

    Ar.Logf( TEXT( "%hs" ), out.c_str() );

    return true;
}

