// (c) 2023 Leonardo F. Juane - To be used under Boost Software License 1.0

//Interaction
#include "Components/InteractionComponent.h"
#include "Components/InteractiveComponent.h"
#include "InteractiveConfig.h"
#include "InteractiveInterface.h"

//Engine
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY(LogInteraction);

UVetInteractionComponent::UVetInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickInterval = 0.25f;

	SetIsReplicatedByDefault(true);
}

void UVetInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	//Do not search for new interactive objects if we are already interacting with something
	if (InteractionState.bIsInteracting)
	{
		return;
	}

	//Don't trace on the server if its not a local player and we are using cursor traces.
	if (TraceType == EVetInteractionTraceType::LineTrace_FromCursor
		&& !IsLocal())
	{
		return;
	}

	TraceForInteractives();
}

void UVetInteractionComponent::StartInteraction()
{
	//We cannot start a new interaction if we are already interacting with something
	if (InteractionState.bIsInteracting)
	{
		return;
	}
	
	if (InteractionState.FocusedActor == nullptr || !IVetInteractiveInterface::Execute_CanBeInteractedWith(InteractionState.FocusedActor, this))
	{
		return;
	}

	if (!GetOwner()->HasAuthority())
	{
		Server_StartInteraction();
		return;
	}
	
	if (UVetInteractiveComponent* InteractiveComponent =  IVetInteractiveInterface::Execute_GetInteractiveComponent(InteractionState.FocusedActor))
	{
		FOnInteractionComplete InteractionCompleteDelegate;
		InteractionCompleteDelegate.BindUObject(this, &UVetInteractionComponent::OnInteractionCompleted);

		if (InteractiveComponent->StartInteraction(*this, InteractionCompleteDelegate))
		{
			//If the interaction is not instant disable ticking to avoid changing focus during the interaction.
			UVetInteractiveConfig* InteractiveConfig = InteractiveComponent->GetInteractiveConfig();
			if (InteractiveConfig && InteractiveConfig->InteractionTime > 0.0f)
			{
				InteractionState.bIsInteracting = true;
				PrimaryComponentTick.SetTickFunctionEnable(false);
			}
		}
	}
}

void UVetInteractionComponent::StartTouchInteraction()
{
	//Don't even try if an interaction is already taking place
	if (InteractionState.bIsInteracting)
	{
		return;
	}

	//Update any focused actor when touching the screen.
	if (TraceType == EVetInteractionTraceType::LineTrace_FromCursor
		&& IsLocal())
	{
		TraceForInteractives(/*bInFromTouch =*/ true);
	}

	StartInteraction();
}

void UVetInteractionComponent::StopInteraction()
{
	//Can't stop an interaction that is not taking place
	if (!InteractionState.bIsInteracting)
	{
		return;
	}

	auto* const FocusedInteractive = Cast<IVetInteractiveInterface>(InteractionState.FocusedActor);
	if (FocusedInteractive == nullptr)
	{
		return;
	}

	UVetInteractiveComponent* InteractiveComponent = IVetInteractiveInterface::Execute_GetInteractiveComponent(InteractionState.FocusedActor);
	if (InteractiveComponent == nullptr)
	{
		return;
	}

	{	//We do nothing if the interaction is not a timed hold interaction since it will complete by itself
		// since it is either instant or a timed not hold interaction.
		UVetInteractiveConfig* InteractiveConfig = InteractiveComponent->GetInteractiveConfig();
		if (InteractiveConfig == nullptr
			|| InteractiveConfig->InteractionTime <= 0.0f
			|| !InteractiveConfig->bIsHoldInteraction)
		{
			return;
		}
	}

	if (!GetOwner()->HasAuthority())
	{
		Server_StopInteraction();
		return;
	}

	if (InteractiveComponent->GetCurrentInteractor() != this)
	{
		//this should never happen. Maybe assert here.
		return;
	}

	InteractionState.bIsInteracting = false;
	InteractionState.Result = EVetInteractionResult::Cancelled;

	InteractiveComponent->CancelInteraction();

	OnInteractionEnded_Internal();
}

void UVetInteractionComponent::BeginPlay()
{
	Super::BeginPlay();	
	
	if (GetOwner()->HasAuthority())
	{
		PrimaryComponentTick.SetTickFunctionEnable(true);
	}
}

void UVetInteractionComponent::Server_StartInteraction_Implementation()
{
	StartInteraction();
}

void UVetInteractionComponent::Server_StopInteraction_Implementation()
{
	StopInteraction();
}

void UVetInteractionComponent::SetFocusedActor(AActor* InNewFocusedActor)
{
	//If we are focusing the same actor as before just exit the function
	//either both null or valid.
	if (InteractionState.FocusedActor == InNewFocusedActor)
	{
		return;
	}

	SwitchFocusedActor(InNewFocusedActor, InteractionState.FocusedActor);

	//We might be removing focus from all actors.
	InteractionState.FocusedActor = InNewFocusedActor;
}

void UVetInteractionComponent::SwitchFocusedActor(AActor* InNewFocusedActor, AActor* InPreviousFocusedActor)
{
	//Remove focus from previous actor
	if (InPreviousFocusedActor != nullptr)
	{
		if (auto* const InteractiveComponent = IVetInteractiveInterface::Execute_GetInteractiveComponent(InPreviousFocusedActor))
		{
			InteractiveComponent->EndFocusedOn();
		}
	}

	//Focus on the new actor
	if (InNewFocusedActor != nullptr)
	{
		if (auto* const InteractiveComponent = IVetInteractiveInterface::Execute_GetInteractiveComponent(InNewFocusedActor))
		{
			InteractiveComponent->BeginFocusedOn();
		}
	}
}

AActor* UVetInteractionComponent::GetClosestActorFromArray(const TArray<AActor*>& InActorArray)
{
	const auto& GetReferenceLocation([&]()
		{
			if (auto* const CameraComponent = GetOwner()->FindComponentByClass<UCameraComponent>())
			{
				return CameraComponent->GetComponentLocation();
			}

			return GetOwner()->GetActorLocation();
		});

	const FVector& ReferenceLocation = GetReferenceLocation();

	auto ClosestActor = TPairInitializer<float, AActor*>(-1.0f, nullptr);
	for (AActor* Actor : InActorArray)
	{
		if (IsValid(Actor))
		{
			const float Distance = FVector(Actor->GetActorLocation() - ReferenceLocation).SizeSquared();
			if (ClosestActor.Value == nullptr
				|| Distance < ClosestActor.Key)
			{
				ClosestActor.Key = Distance;
				ClosestActor.Value = Actor;
			}
		}
	}
	return ClosestActor.Value;
}

void UVetInteractionComponent::OnInteractionCompleted(UVetInteractiveComponent& InInteractive)
{
	if (InteractionState.FocusedActor == nullptr)
	{
		//TODO: log this, this should never happen
		return;
	}

	UVetInteractiveComponent* InteractiveComponent = IVetInteractiveInterface::Execute_GetInteractiveComponent(InteractionState.FocusedActor);
	if (&InInteractive != InteractiveComponent)
	{
		//TODO: log this.
		return;
	}

	InteractionState.bIsInteracting = false;
	InteractionState.Result = EVetInteractionResult::Success;

	OnInteractionEnded_Internal();
}

void UVetInteractionComponent::OnInteractionEnded_Internal()
{
	UVetInteractiveComponent* CurrentInteractive = IVetInteractiveInterface::Execute_GetInteractiveComponent(InteractionState.FocusedActor);
	OnInteractionEnded.Broadcast(CurrentInteractive, InteractionState.Result);

	//Re-enable ticking on the server
	if (GetOwner()->HasAuthority()
		&& !PrimaryComponentTick.IsTickFunctionEnabled())
	{
		PrimaryComponentTick.SetTickFunctionEnable(true);
	}
}

void UVetInteractionComponent::TraceForInteractives(bool bInFromTouch /*= false*/)
{
	TArray<FHitResult> HitResults;
	TArray<AActor*> ActorsToIgnore{ GetOwner() };

	if (TraceType == EVetInteractionTraceType::LineTrace_FromCursor)
	{
		GetTraceHitForLocalPlayerCursor(HitResults.Emplace_GetRef());
	}
	else
	{
		//Multi sphere trace from owning actor
		const FVector StartLocation = GetOwner()->GetActorLocation();
		const FVector EndLocation = StartLocation + (GetOwner()->GetActorForwardVector() * InteractionDistance);
		UKismetSystemLibrary::SphereTraceMulti(this, StartLocation, EndLocation, InteractionRadius, UEngineTypes::ConvertToTraceType(TraceChannel), false, ActorsToIgnore, EDrawDebugTrace::None, HitResults, true);
	}

	if (HitResults.Num() > 0)
	{
		TArray<AActor*> FoundInteractives;
		for (const FHitResult& HitResult : HitResults)
		{
			AActor* const HitActor = HitResult.GetActor();
			if (HitActor != nullptr
				&& HitActor->GetClass()->ImplementsInterface(UVetInteractiveInterface::StaticClass())
				&& IVetInteractiveInterface::Execute_CanBeFocusedOn(HitActor, this))	//Ignore interactives that cannot be focused on
			{
				FoundInteractives.Emplace(HitActor);
			}
		}

		AActor* const ClosestInteractive = GetClosestActorFromArray(FoundInteractives);
		SetFocusedActor(ClosestInteractive);
	}
	else
	{
		SetFocusedActor(nullptr);
	}
}

void UVetInteractionComponent::GetTraceHitForLocalPlayerCursor(FHitResult& OutResult, bool bInFromTouch /*= false*/) const
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!IsValid(PC) || !PC->IsLocalController())
	{
		return;
	}

	if (bInFromTouch)
	{
		PC->GetHitResultUnderFinger(ETouchIndex::Touch1, TraceChannel, /*bTraceComplex =*/ true, OutResult);
	}
	else
	{
		PC->GetHitResultUnderCursor(TraceChannel, /*bTraceComplex =*/ true, OutResult);
	}
}

void UVetInteractionComponent::OnRep_InteractionState(const FVetInteractionComponentState& InPreviousState)
{
	if (InPreviousState.bIsInteracting != InteractionState.bIsInteracting)
	{
		UVetInteractiveComponent* CurrentInteractive = InteractionState.FocusedActor == nullptr ? nullptr : IVetInteractiveInterface::Execute_GetInteractiveComponent(InteractionState.FocusedActor);

		if (InteractionState.bIsInteracting)
		{
			OnInteractionStarted.Broadcast(CurrentInteractive);
		}
		else
		{
			OnInteractionEnded.Broadcast(CurrentInteractive, InteractionState.Result);
		}
	}

	if (InteractionState.FocusedActor != InPreviousState.FocusedActor)
	{
		SwitchFocusedActor(InteractionState.FocusedActor, InPreviousState.FocusedActor);
	}
}

bool UVetInteractionComponent::IsLocal() const
{

	APlayerController* const PC = GetWorld()->GetFirstPlayerController();
	return IsValid(PC) && PC->IsLocalController();
}

void UVetInteractionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UVetInteractionComponent, InteractionState);
}