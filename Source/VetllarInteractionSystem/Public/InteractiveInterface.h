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

DECLARE_LOG_CATEGORY_EXTERN(LogVetInteractiveInterface, Log, All);

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
	
	// Native --------------------------------------------------------------------------------------------------- //

	virtual EVetInteractability GetInteractabilityState() const { return EVetInteractability::Available; }
	virtual bool CanBeInteractedWith(UVetInteractionComponent& InInteractor) const { return true; }
	virtual bool CanBeFocusedOn(UVetInteractionComponent& InInteractor) const { return true; }
	virtual UVetInteractiveComponent* GetInteractiveComponent() const { return nullptr; }

	//Blueprint functions --------------------------------------------------------------------------------------- //

	//Allows blueprints to state a desired interactability for this actor.
	//The result will be the less available of the states based on this function and internal results.
	//(e.g: if this function returns available but internally the result is unavailable the interactability will
	// be unavailable).
	//Params:
	//@bOutFunctionImplemented - In blueprints this MUST return true, otherwise the result of this function
	//will be ignored (this is because since 5.0 interface functions are implemented by default in blueprints).
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, meta = (DisplayName = "GetInteractabilityState"))
	EVetInteractability K2_GetDesiredInteractabilityState(bool& bOutFunctionImplemented) const;

	//Returns true if the interactor is allowed to interact with this actor.
	//Params:
	//@InInteractor - The interaction component trying to interact with this actor
	//@bOutFunctionImplemented - In blueprints this MUST return true, otherwise the result of this function
	//will be ignored (this is because since 5.0 interface functions are implemented by default in blueprints).
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, meta = (DisplayName = "CanBeInteractedWith"))
	bool K2_CanBeInteractedWith(UVetInteractionComponent* InInteractor, bool& bOutFunctionImplemented) const;

	//Returns true if the interactor is allowed to focus on this actor.
	//Params:
	//@InInteractor - The interaction component trying to interact with this actor
	//@bFuncionImplemented - In blueprints this MUST return true, otherwise the result of this function
	//will be ignored (this is because since 5.0 interface functions are implemented by default in blueprints).
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, meta = (DisplayName = "CanBeFocusedOn"))
	bool K2_CanBeFocusedOn(UVetInteractionComponent* InInteractor, bool& bOutFunctionImplemented) const;

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, meta = (DisplayName = "GetInteractiveComponent"))
	UVetInteractiveComponent* K2_GetInteractiveComponent() const;

	// ----------------------------------------------------------------------------------------------------------- //

private:

	friend class UVetInteractionComponent;

	static EVetInteractability GetInteractabilityState_Internal(AActor* InInteractive);
	static bool CanBeInteractedWith_Internal(AActor* InInteractive, UVetInteractionComponent* InInteractor);
	static bool CanBeFocusedOn_Internal(AActor* InInteractive, UVetInteractionComponent* InInteractor);
	static UVetInteractiveComponent* GetInteractiveComponent_Internal(AActor* InInteractive);
};