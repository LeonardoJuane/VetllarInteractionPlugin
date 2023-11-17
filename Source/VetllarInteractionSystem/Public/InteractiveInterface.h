// (c) 2022 Property of Leonardo F. Juane - All rights reserved

#pragma once

//Engine
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/Interface.h"

//Interaction
#include "InteractiveTypes.h"
#include "InteractiveInterface.generated.h"

class UVetInteractionComponent;
class UVetInteractiveComponent;

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UVetInteractiveInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 *  Interface for actors that can be interacted with
 */
class VETLLARINTERACTIONSYSTEM_API IVetInteractiveInterface
{
	GENERATED_BODY()

public:

	//Blueprint functions

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	EVetInteractability GetInteractabilityState() const;
	virtual EVetInteractability GetInteractabilityState_Implementation() const;

	//checks if the input interactor should be allowed to interact with this interactive
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	bool CanBeInteractedWith(UVetInteractionComponent* InInteractor) const;
	virtual bool CanBeInteractedWith_Implementation(UVetInteractionComponent* InInteractor) const;

	//checks if the input interactor should be allowed to focus on this interactive
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	bool CanBeFocusedOn(UVetInteractionComponent* InInteractor) const;
	bool CanBeFocusedOn_Implementation(UVetInteractionComponent* InInteractor) const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	UVetInteractiveComponent* GetInteractiveComponent() const;
	virtual UVetInteractiveComponent* GetInteractiveComponent_Implementation() const;
};

/**
 * Helper function library for blueprint implemented interactive interface
 */
 UCLASS(BlueprintType, NotBlueprintable)
 class VETLLARINTERACTIONSYSTEM_API UVetInteractiveHelperLibrary : public UBlueprintFunctionLibrary
 {
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintPure)
	static EVetInteractability Native_GetInteractabilityState(TScriptInterface<IVetInteractiveInterface> InteractiveObject);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	static bool Native_CanbeInteractedWith(TScriptInterface<IVetInteractiveInterface> InteractiveObject, UVetInteractionComponent* InInteractor);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	static bool Native_CanBeFocusedOn(TScriptInterface<IVetInteractiveInterface> InteractiveObject, UVetInteractionComponent* InInteractor);
 };