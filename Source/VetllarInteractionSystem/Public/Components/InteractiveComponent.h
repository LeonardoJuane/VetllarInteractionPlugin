// (c) 2023 Leonardo F. Juane - To be used under Boost Software License 1.0

#pragma once

//Engine
#include "Components/ActorComponent.h"
#include "Components/PrimitiveComponent.h"

//Interaction
#include "InteractiveConfig.h"
#include "InteractiveTypes.h"
#include "InteractiveComponent.generated.h"

class UVetInteractionComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogVetInteractive, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractabilityStateChanged, EVetInteractability, NewInteractabilityState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteractionStarted_Multicast, UVetInteractionComponent*, InInteractor, UPrimitiveComponent*, FocusedOnComponent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnInteractionEnded_Multicast, UVetInteractionComponent*, InInteractor, EVetInteractionResult, InteractionResult, UPrimitiveComponent*, FocusedOnComponent);

DECLARE_DELEGATE_OneParam(FOnInteractionComplete, UVetInteractiveComponent& /*this*/);

USTRUCT()
struct VETLLARINTERACTIONSYSTEM_API FVetInteractiveState
{
	GENERATED_BODY()

	FORCEINLINE void SetIsBeingInteractedWith(bool bInNewIsBeingInteractedWith, UPrimitiveComponent* InFocusedOnComponent = nullptr)
	{
		bIsBeingInteractedWith = bInNewIsBeingInteractedWith;
		
		//We want to keep this valid since during an instant interaction this will not replicate
		//correctly if nulled after broadcasting on the server.
		if (IsValid(InFocusedOnComponent)
			&& InFocusedOnComponent != FocuedOnComponent)
		{
			FocuedOnComponent = InFocusedOnComponent;
		}

		ReplicationKey++;
	}

	bool IsBeingInteractedWith() const { return bIsBeingInteractedWith; }
	uint64 GetReplicationKey() const { return ReplicationKey; }
	UPrimitiveComponent* GetFocusedOnComponent() const { return FocuedOnComponent.Get(); }

	UPROPERTY()
	EVetInteractability InteractabilityState{EVetInteractability::Available};

	UPROPERTY()
	EVetInteractionResult InteractionResult{EVetInteractionResult::Success};

private:

	UPROPERTY()
	bool bIsBeingInteractedWith{false};

	//The component that was being focused on when the interaction started
	UPROPERTY()
	TWeakObjectPtr<UPrimitiveComponent> FocuedOnComponent;

	UPROPERTY()
	uint64 ReplicationKey{0};
};

UCLASS( ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent) )
class VETLLARINTERACTIONSYSTEM_API UVetInteractiveComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UVetInteractiveComponent();

	bool CanBeFocusedOn(UVetInteractionComponent& InInteractor) const;
	bool CanBeInteractedWith(UVetInteractionComponent& InInteractor) const;
	bool StartInteraction(UVetInteractionComponent& InInteractor, const FOnInteractionComplete& InCompleteDelegate, UPrimitiveComponent* InFocuedOnComponent);
	void CancelInteraction();
		
	void BeginFocusedOn(UVetInteractionComponent& InInteractor, UPrimitiveComponent* InFocusedOnComponent);
	void EndFocusedOn(UVetInteractionComponent& InInteractor, UPrimitiveComponent* InFocusedOnComponent);

	EVetInteractability GetInteractabilityState() const { return InteractiveState.InteractabilityState; }
	UVetInteractionComponent* GetCurrentInteractor() { return CurrentInteractor.Get(); }
	UVetInteractiveConfig* GetInteractiveConfig() { return InteractiveConfig; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void SetIsEnabled(bool bInNewEnabled);

	//Gets the progress of the current timed interaction as a percent.
	//Returns false if this is not a timed interactive or if no interaction is taking place.
	//Outs a value between 0 and 1 based on the elapsed time and the required time.
	UFUNCTION(BlueprintCallable)
	bool GetCurrentInteractionAsPercent(float& OutPercent) const;

	//Gets the amount of time remaining to finish an interaction.
	//Returns false if this is not a timed interactive or if no interaction is taking place
	//Outs the time left to finish the interaction and the originally required time
	UFUNCTION(BlueprintCallable)
	bool GetCurrentInteractionRemainingTime(float& OutRemainingTime, float& OutRequiredTime) const;

	UFUNCTION(BlueprintCallable)
	bool IsBeingInteractedWith() const { return InteractiveState.IsBeingInteractedWith(); }

	UPROPERTY(BlueprintAssignable)
	FOnInteractabilityStateChanged OnInteractabilityStateChanged;

	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "On Interaction Started"))
	FOnInteractionStarted_Multicast K2_OnInteractionStarted;

	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "On Interaction Ended"))
	FOnInteractionEnded_Multicast K2_OnInteractionEnded;

protected:

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Begin Focused On"))
	void K2_OnBeginFocusedOn(UVetInteractionComponent* InInteractor, UPrimitiveComponent* InFocusedOnComponent);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On End Focused On"))
	void K2_OnEndFocusedOn(UVetInteractionComponent* InInteractor, UPrimitiveComponent* InFocusedOnComponent);

	virtual void OnBeginFocusedOn(UVetInteractionComponent& InInteractor, UPrimitiveComponent* InFocusedOnComponent) {}
	virtual void OnEndFocusedOn(UVetInteractionComponent& InInteractor, UPrimitiveComponent* InFocusedOnComponent) {}

	//Set to true if you want this interactive to be available on start.
	UPROPERTY(EditAnywhere)
	bool bEnabled{true};

	//The configuration required for this interaction to take place
	UPROPERTY(EditAnywhere)
	TObjectPtr<UVetInteractiveConfig> InteractiveConfig;

private:

	void EvaluateInteractabilityState_Internal();

	UFUNCTION()
	void OnRep_InteractiveState(const FVetInteractiveState& InPreviousState);

	void OnInteractionStarted();
	void OnInteractionEnded(EVetInteractionResult InResult);

	void CompleteInteraction_Internal();
	void EndInteraction_Internal();

	UPROPERTY(ReplicatedUsing = OnRep_InteractiveState)
	FVetInteractiveState InteractiveState;

	UPROPERTY(Transient)
	TObjectPtr<UVetInteractivePrerequisiteScript> InteractionPrerequisiteScript;

	//Valid if this is being interacted with
	TWeakObjectPtr<UVetInteractionComponent> CurrentInteractor;

	//The time that passed since the interaction started.
	//NOT REPLICATED. This value is predicted on clients for cosmetic purposes only.
	float InteractionElapsedTime{0.0f};

	//Used to notify the interaction component that the interaction ended
	FOnInteractionComplete OnInteractionComplete;
};
