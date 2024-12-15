# YamlDataAsset

A plugin for Unreal Editor that allows convert between UDataAssets  YAML files.

This makes it easier to work with custom data from external sources.

## Installation

* Like any plugin, clone this repository into the `Plugins` directory and compile into your project.

* You may also need to enable it in the plugins settings (`Edit->Plugins->Project->Editor`).

 
## Usage

### To Import

* Drop a YAML file into the content browser.
* Select the asset type to conver to from the list.
* Click OK.

![Select Asset](./Docs/SelectAsset.png)

### To Export

* Choose export from the `Asset Actions` menu, choose YAML.

![Export Asset](./Docs/Export.png)


## Example

### UMyDataAsset

```c++
UCLASS( BlueprintType )
class UMyDataAsset : public UDataAsset
{
	GENERATED_UCLASS_BODY()

	UPROPERTY( EditAnywhere )
	FName Name;

	UPROPERTY( EditAnywhere )
	TArray<int> SomeNumbers;
};
```

### YAML

```yaml
Name: MyName
SomeNumbers:
  - 4
  - 8
  - 15
  - 16
  - 23
  - 42
```

The plugin uses the Unreal reflection system walk the properties and set the values. It is recursive, so it will work with nested structures and compound types.

## Notes

### __uclass

As a convenience, you set the class type as a `__uclass` property at the top of the file, to let the plugin know the class to use. This will mean there is no need to bring up the dialog box. For example:

```yaml
__uclass: MyDataAsset
Name: MyName
SomeNumbers:
  - 4
  - 8
  - 15
  - 16
  - 23
  - 42
```

### References

Pointer types (e.g. `TWeakObjectPtr<UObject>`) can be set if as asset reference (right click, "Copy Reference" in the content browser). For example:

```yaml
MyObjectRef: /Script/GameJam3.MyDataAsset'/Game/Assets/MyDataAsset.MyDataAsset'

```

### TMap Compound Keys

`TMap`'s with a compound keys (structs as keys) is not supported. Whlist this is allowed in Unreal, YAML/JSON only allow value types in the file format (JavaScript limitation).

