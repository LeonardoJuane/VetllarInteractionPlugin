// (c) 2023 Leonardo F. Juane - To be used under Boost Software License 1.0

//Interaction
#include "Components/InteractiveComponent.h"
#include "Components/InteractionComponent.h"

//Engine
#include "Net/UnrealNetwork.h"

UVetInteractiveComponent::UVetInteractiveComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	SetIsReplicatedByDefault(true);
}

bool UVetInteractiveComponent::CanBeFocusedOn(UVetInteractionComponent& InInteractor) const
{
	if (InteractiveConfig == nullptr)
	{
		return false;
	}

	bool bResult = InteractiveState.InteractabilityState != EVetInteractability::Unavailable;
	if (InteractionPrerequisiteScript != nullptr)
	{
		bResult = bResult && InteractionPrerequisiteScript->CanBeFocusedOn(InInteractor);
	}
	return bResult;
}

bool UVetInteractiveComponent::CanBeInteractedWith(UVetInteractionComponent& InInteractor) const
{
	//there is no way to know how this interaction should behave!
	if (InteractiveConfig == nullptr)
	{
		return false;
	}

	bool bResult = InteractiveState.InteractabilityState == EVetInteractability::Available;
	if (InteractionPrerequisiteScript != nullptr)
	{
		bResult = bResult && InteractionPrerequisiteScript->CanBeInteractedWith(InInteractor);
	}
	return bResult;
}

bool UVetInteractiveComponent::StartInteraction(UVetInteractionComponent& InInteractor, const FOnInteractionComplete& InCompleteDelegate)
{
	check(GetOwner()->HasAuthority());

	if (!CanBeInteractedWith(InInteractor))
	{
		return false;
	}

	CurrentInteractor = &InInteractor;
	InteractiveState.SetIsBeingInteractedWith(true);

	OnInteractionComplete = InCompleteDelegate;

	OnInteractionStarted();
	return true;
}

void UVetInteractiveComponent::CancelInteraction()
{
	check(GetOwner()->HasAuthority());
	EndInteraction_Internal();
}

void UVetInteractiveComponent::BeginFocusedOn()
{
	OnBeginFocusedOn();
	K2_OnBeginFocusedOn();
}

void UVetInteractiveComponent::EndFocusedOn()
{
	OnEndFocusedOn();
	K2_OnEndFocusedOn();
}

void UVetInteractiveComponent::SetIsEnabled(bool bInNewEnabled)
{
	check(GetOwner()->HasAuthority());

	if (bInNewEnabled != bEnabled)
	{
		bEnabled = bInNewEnabled;
		EvaluateInteractabilityState_Internal();
	}
}

bool UVetInteractiveComponent::GetCurrentInteractionAsPercent(float& OutPercent) const
{
	if (InteractiveState.IsBeingInteractedWith() == false
		|| InteractiveConfig == nullptr || InteractiveConfig->InteractionTime <= 0.0f)
	{
		OutPercent = 0.0f;
		return false;
	}

	OutPercent = InteractionElapsedTime / InteractiveConfig->InteractionTime;
	return true;
}

bool UVetInteractiveComponent::GetCurrentInteractionRemainingTime(float& OutRemainingTime, float& OutRequiredTime) const
{
	if (InteractiveState.IsBeingInteractedWith() == false
		|| InteractiveConfig == nullptr || InteractiveConfig->InteractionTime <= 0.0f)
	{
		OutRequiredTime = 0.0f;
		OutRemainingTime = 0.0f;
		return false;
	}

	OutRequiredTime = InteractiveConfig->InteractionTime;
	OutRemainingTime =  OutRequiredTime - InteractionElapsedTime;
	return true;
	
}

void UVetInteractiveComponent::BeginPlay()
{
	Super::BeginPlay();

	//TODO: log the lack of interictive config!!!!!!!
	EvaluateInteractabilityState_Internal();

	if (InteractiveConfig != nullptr && InteractiveConfig->PrerequisitesScript != nullptr)
	{
		InteractionPrerequisiteScript = NewObject<UVetInteractivePrerequisiteScript>(this, InteractiveConfig->PrerequisitesScript, TEXT("Prerequisites Script"));
	}
}

void UVetInteractiveComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (GetOwner()->HasAuthority()
		&& (!InteractiveState.IsBeingInteractedWith() || !CurrentInteractor.IsValid()))
	{
		CancelInteraction();
	}

	InteractionElapsedTime += DeltaTime;
	if (InteractionElapsedTime >= InteractiveConfig->InteractionTime)
	{
		//Avoid it going over the max value, this is mostly to avoid issues on the client side.
		InteractionElapsedTime = InteractiveConfig->InteractionTime;

		//Call interaction ended events
		CompleteInteraction_Internal();
	}
}

void UVetInteractiveComponent::EvaluateInteractabilityState_Internal()
{
	EVetInteractability PrevInteractabilityState = InteractiveState.InteractabilityState;
	InteractiveState.InteractabilityState = EVetInteractability::Unavailable;
	if (InteractiveConfig != nullptr)
	{
		if (CurrentInteractor.IsValid())
		{
			InteractiveState.InteractabilityState = EVetInteractability::FocusableButUnavailable;
		}
		else if (bEnabled)
		{
			InteractiveState.InteractabilityState = EVetInteractability::Available;
		}
	}

	if (PrevInteractabilityState != InteractiveState.InteractabilityState)
	{
		OnInteractabilityStateChanged.Broadcast(InteractiveState.InteractabilityState);
	}
}

void UVetInteractiveComponent::OnRep_InteractiveState(const FVetInteractiveState& InPreviousState)
{
	if (InteractiveState.InteractabilityState != InPreviousState.InteractabilityState)
	{
		OnInteractabilityStateChanged.Broadcast(InteractiveState.InteractabilityState);
	}

	if (InteractiveState.IsBeingInteractedWith() != InPreviousState.IsBeingInteractedWith()
		|| InteractiveState.GetReplicationKey() != InPreviousState.GetReplicationKey())
	{
		if (InteractiveState.IsBeingInteractedWith())
		{
			OnInteractionStarted();
		}
		else
		{
			OnInteractionEnded(InteractiveState.InteractionResult);
		}
	}
}

void UVetInteractiveComponent::OnInteractionStarted()
{
	K2_OnInteractionStarted.Broadcast(CurrentInteractor.Get());

	if (InteractiveConfig->InteractionTime > 0.0f)
	{
		PrimaryComponentTick.SetTickFunctionEnable(true);
	}
	//If the interaction is instant and we are on the server, end the interaction instantly
	else if (GetOwner()->HasAuthority())
	{
		CompleteInteraction_Internal();
	}
}

void UVetInteractiveComponent::OnInteractionEnded(EVetInteractionResult InResult)
{
	PrimaryComponentTick.SetTickFunctionEnable(false);
	InteractionElapsedTime = 0.0f;

	K2_OnInteractionEnded.Broadcast(CurrentInteractor.Get(), InResult);
}

void UVetInteractiveComponent::CompleteInteraction_Internal()
{
	if (GetOwner()->HasAuthority())
	{
		OnInteractionComplete.Execute(*this);
		OnInteractionComplete.Unbind();
	
		EndInteraction_Internal();
	}
}

void UVetInteractiveComponent::EndInteraction_Internal()
{
	OnInteractionEnded(InteractiveState.InteractionResult);
	CurrentInteractor = nullptr;
	InteractiveState.SetIsBeingInteractedWith(false);
}

void  UVetInteractiveComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UVetInteractiveComponent, InteractiveState);
}
