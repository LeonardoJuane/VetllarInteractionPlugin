// (c) 2023 Leonardo F. Juane - To be used under Boost Software License 1.0

#pragma once

//Engine
#include "Engine/DataAsset.h"

//Interaction
#include "InteractiveConfig.generated.h"

class UVetInteractionComponent;
class UVetInteractiveComponent;

/**
 * Executed to check if the interactor fulfills the prerequisites for this interaction
 * Avoid referencing heavy assets in this class at all cost! this is just meant to execute logic and nothing else.
 */
UCLASS(BlueprintType, Blueprintable)
class VETLLARINTERACTIONSYSTEM_API UVetInteractivePrerequisiteScript : public UObject
{
	GENERATED_BODY()

public:
	virtual bool CanBeFocusedOn(const UVetInteractionComponent& InInteractor) const;
	virtual bool CanBeInteractedWith(const UVetInteractionComponent& InInteractor) const;

protected:
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "Can be Focused on"))
	bool K2_CanBeFocusedOn(const UVetInteractionComponent* InInteractor) const;

	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "Can be Interacted with"))
	bool K2_CanBeInteractedWith(const UVetInteractionComponent* InInteractor) const;

	UFUNCTION(BlueprintCallable)
	UVetInteractiveConfig* GetInteractiveConfig() const;

	UFUNCTION(BlueprintCallable)
	UVetInteractiveComponent* GetInteractiveComponent() const;
};

/**
 * Configuration for interactive actors.
 * this allows to share the same config across multiple actor classes
 */
UCLASS()
class VETLLARINTERACTIONSYSTEM_API UVetInteractiveConfig : public UDataAsset
{
	GENERATED_BODY()
	
public:

	//The name of the interaction, this is solely to identify it.
	UPROPERTY(EditDefaultsOnly)
	FName InteractionName{NAME_None};

	//The name of the action that takes place when the interaction starts
	//This is used to format the interaction prompt. E.g: "Press E to Interact".
	UPROPERTY(EditDefaultsOnly)
	FText ActionName = NSLOCTEXT("Interactions namespace", "InteractActionName", "Interact");

	//The time it takes for the interaction to complete
	//0 or less means the interaction is instant.
	UPROPERTY(EditDefaultsOnly)
	float InteractionTime{0.0f};

	//True if the interaction button is required to be held until the interaction completes
	//Ignored if the interaction is instant
	UPROPERTY(EditDefaultsOnly)
	bool bIsHoldInteraction{false};

	//Show the interaction as unavailable if the requisites are not met?
	//Ignored if not requisites exists
	UPROPERTY(EditDefaultsOnly)
	bool bUnavailableIfRequisitesNotMet{false};

	// Script class to check if the pre requisites to execute this interaction
	// are met.
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UVetInteractivePrerequisiteScript> PrerequisitesScript;
};
