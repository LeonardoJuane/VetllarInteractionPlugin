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
	if (InteractionState.IsInteracting())
	{
		return;
	}

	TraceForInteractives();
}

void UVetInteractionComponent::StartInteraction()
{
	//We cannot start a new interaction if we are already interacting with something
	if (InteractionState.IsInteracting())
	{
		return;
	}
	
	if (!IsValid(InteractionState.GetFocusedComponent()) || !IVetInteractiveInterface::CanBeInteractedWith_Internal(InteractionState.GetFocusedActor(), this))
	{
		return;
	}

	if (!GetOwner()->HasAuthority())
	{
		//Pass in the focused actor in case we predicted it during a trace from cursor (the server can't know the focused actor if tracing from cursor).

		UPrimitiveComponent* ServerFocusedComponent = (TraceType == EVetInteractionTraceType::LineTrace_FromCursor) ? InteractionState.GetFocusedComponent() : nullptr;
		Server_StartInteraction(ServerFocusedComponent);	
		return;
	}
	
	if (UVetInteractiveComponent* InteractiveComponent =  IVetInteractiveInterface::GetInteractiveComponent_Internal(InteractionState.GetFocusedActor()))
	{
		FOnInteractionComplete InteractionCompleteDelegate;
		InteractionCompleteDelegate.BindUObject(this, &UVetInteractionComponent::OnInteractionCompleted);

		if (InteractiveComponent->StartInteraction(*this, InteractionCompleteDelegate, InteractionState.GetFocusedComponent()))
		{
			//If the interaction is not instant disable ticking to avoid changing focus during the interaction.
			UVetInteractiveConfig* InteractiveConfig = InteractiveComponent->GetInteractiveConfig();
			if (InteractiveConfig && InteractiveConfig->InteractionTime > 0.0f)
			{
				InteractionState.SetIsInteracting(true);
				ConditionallySetTickEnabled(/*bInEnabled =*/ false);
			}
		}
	}
}

void UVetInteractionComponent::StartTouchInteraction()
{
	//Don't even try if an interaction is already taking place
	if (InteractionState.IsInteracting())
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
	if (!InteractionState.IsInteracting())
	{
		return;
	}

	auto* const FocusedInteractive = Cast<IVetInteractiveInterface>(InteractionState.GetFocusedActor());
	if (FocusedInteractive == nullptr)
	{
		return;
	}

	UVetInteractiveComponent* InteractiveComponent = IVetInteractiveInterface::GetInteractiveComponent_Internal(InteractionState.GetFocusedActor());
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

	InteractionState.SetIsInteracting(false);
	InteractionState.SetResult(EVetInteractionResult::Cancelled);

	InteractiveComponent->CancelInteraction();

	OnInteractionEnded_Internal();
}

bool UVetInteractionComponent::IsLocallyControlled() const
{
	AActor* const OwningActor = GetOwner();
	if (!IsValid(OwningActor))
	{
		return false;
	}

	if (OwningActor->IsA<APawn>())
	{
		return CastChecked<APawn>(OwningActor)->IsLocallyControlled();
	}
	else if (OwningActor->IsA<AController>())
	{
		return CastChecked<AController>(OwningActor)->IsLocalController();
	}
	return false;
}

void UVetInteractionComponent::SetDefaultTraceChannel(ECollisionChannel InTraceChannel)
{
	if (CheckConstructorContext(TEXT("SetDefaultTraceChannel")))
	{
		TraceChannel = InTraceChannel;
	}
}

void UVetInteractionComponent::SetDefaultTraceType(EVetInteractionTraceType InTraceType)
{
	if (CheckConstructorContext(TEXT("SetDefaultTraceType")))
	{
		TraceType = InTraceType;
	}
}

void UVetInteractionComponent::SetDefaultInteractionDistance(float InInteractionDistance)
{
	if (CheckConstructorContext(TEXT("SetDefaultInteractionDistance")))
	{
		InteractionDistance = InInteractionDistance;
	}
}

void UVetInteractionComponent::SetDefaultInteractionRadius(float InInteractionRadius)
{
	if (CheckConstructorContext(TEXT("SetDefaultInteractionRadius")))
	{
		InteractionRadius = InInteractionRadius;
	}
}

void UVetInteractionComponent::BeginPlay()
{
	Super::BeginPlay();	
	
	ConditionallySetTickEnabled(/*bInEnabled =*/ true);
}

void UVetInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetOwner()->HasAuthority()
		&& InteractionState.IsInteracting())
	{
		StopInteraction();
	}

	if (IsValid(InteractionState.GetFocusedComponent()))
	{
		SetFocusedComponent(nullptr);
	}

	Super::EndPlay(EndPlayReason);
}

bool UVetInteractionComponent::Server_StartInteraction_Validate(UPrimitiveComponent* InFocusedComponent)
{
	if (TraceType == EVetInteractionTraceType::LineTrace_FromCursor)
	{
		return IsValid(InFocusedComponent);
	}
	return true;
}

void UVetInteractionComponent::Server_StartInteraction_Implementation(UPrimitiveComponent* InFocusedComponent)
{
	if (IsValid(InFocusedComponent))
	{
		SetFocusedComponent(InFocusedComponent);
	}

	StartInteraction();
}

void UVetInteractionComponent::Server_StopInteraction_Implementation()
{
	StopInteraction();
}

void UVetInteractionComponent::SetFocusedComponent(UPrimitiveComponent* InNewFocusedComponent)
{
	//If we are focusing the same actor as before just exit the function
	//either both null or valid.
	if (InteractionState.GetFocusedComponent() == InNewFocusedComponent)
	{
		return;
	}

	SwitchFocusedComponent(InNewFocusedComponent, InteractionState.GetFocusedComponent());

	//We might be removing focus from all actors.
	InteractionState.SetFocusedComponent(InNewFocusedComponent);
}

void UVetInteractionComponent::SwitchFocusedComponent(UPrimitiveComponent* InNewFocusedComponent, UPrimitiveComponent* InPreviousFocusedComponent)
{
	//Remove focus from previous actor
	if (InPreviousFocusedComponent != nullptr)
	{
		AActor* PreviousFocusedActor = InPreviousFocusedComponent->GetOwner();
		IVetInteractiveInterface::EndFocusedOn_Internal(*this, PreviousFocusedActor, InPreviousFocusedComponent);
	}

	//Focus on the new actor
	if (InNewFocusedComponent != nullptr)
	{
		AActor* NewFocusedActor = InNewFocusedComponent->GetOwner();
		IVetInteractiveInterface::BeginFocusedOn_Internal(*this, NewFocusedActor, InNewFocusedComponent);
	}
}

UPrimitiveComponent* UVetInteractionComponent::GetClosestPrimitiveFromArray(const TArray<UPrimitiveComponent*>& InPrimitivesArray)
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

	auto ClosestActor = TPairInitializer<float, UPrimitiveComponent*>(-1.0f, nullptr);
	for (UPrimitiveComponent* Primitive : InPrimitivesArray)
	{
		if (IsValid(Primitive))
		{
			const float Distance = FVector(Primitive->GetComponentLocation() - ReferenceLocation).SizeSquared();
			if (ClosestActor.Value == nullptr
				|| Distance < ClosestActor.Key)
			{
				ClosestActor.Key = Distance;
				ClosestActor.Value = Primitive;
			}
		}
	}
	return ClosestActor.Value;
}

void UVetInteractionComponent::OnInteractionCompleted(UVetInteractiveComponent& InInteractive)
{
	if (InteractionState.GetFocusedActor() == nullptr)
	{
		//TODO: log this, this should never happen
		return;
	}

	UVetInteractiveComponent* InteractiveComponent = IVetInteractiveInterface::GetInteractiveComponent_Internal(InteractionState.GetFocusedActor());
	if (&InInteractive != InteractiveComponent)
	{
		//TODO: log this.
		return;
	}

	InteractionState.SetIsInteracting(false);
	InteractionState.SetResult(EVetInteractionResult::Success);

	OnInteractionEnded_Internal();
}

void UVetInteractionComponent::OnInteractionEnded_Internal()
{
	UVetInteractiveComponent* CurrentInteractive = IVetInteractiveInterface::GetInteractiveComponent_Internal(InteractionState.GetFocusedActor());
	OnInteractionEnded.Broadcast(CurrentInteractive, InteractionState.GetResult());

	//Re-enable ticking on the server
	if (!PrimaryComponentTick.IsTickFunctionEnabled())
	{
		ConditionallySetTickEnabled(/*bInEnabled =*/ true);
	}
}

void UVetInteractionComponent::TraceForInteractives(bool bInFromTouch /*= false*/)
{
	TArray<FHitResult> HitResults;
	TArray<AActor*> ActorsToIgnore{ GetOwner() };

	if (TraceType == EVetInteractionTraceType::LineTrace_FromCursor)
	{
		FHitResult& HitResult = HitResults.Emplace_GetRef();
		GetTraceHitForLocalPlayerCursor(HitResult);
#if WITH_EDITOR
		if (IsValid(HitResult.GetActor()))
		{
			PrintDebugMessage(0, FString::Printf(TEXT("Hit Actor: %s"), *HitResult.GetActor()->GetName()));
		}
#endif //WITH_EDITOR
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
		TArray<UPrimitiveComponent*> FoundInteractives;
		for (const FHitResult& HitResult : HitResults)
		{
			AActor* const HitActor = HitResult.GetActor();
			const bool bDoesImplementInterface = UKismetSystemLibrary::DoesImplementInterface(HitActor, UVetInteractiveInterface::StaticClass());
			if (IsValid(HitActor)
				&& bDoesImplementInterface
				&& IVetInteractiveInterface::CanBeFocusedOn_Internal(HitActor, this))	//Ignore interactives that cannot be focused on
			{
				FoundInteractives.Emplace(HitResult.GetComponent());
			}
		}

		UPrimitiveComponent* const ClosestInteractive = GetClosestPrimitiveFromArray(FoundInteractives);
		SetFocusedComponent(ClosestInteractive);
	}
	else
	{
		SetFocusedComponent(nullptr);
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

void UVetInteractionComponent::ConditionallySetTickEnabled(bool bInEnabled)
{
	if ((TraceType == EVetInteractionTraceType::LineTrace_FromCursor && IsLocal())
		|| TraceType != EVetInteractionTraceType::LineTrace_FromCursor && GetOwner()->HasAuthority())
	{
		PrimaryComponentTick.SetTickFunctionEnable(bInEnabled);
	}
}

void UVetInteractionComponent::OnRep_InteractionState(const FVetInteractionComponentState& InPreviousState)
{
	if (InPreviousState.IsInteracting() != InteractionState.IsInteracting()
		|| InPreviousState.GetReplicationKey() != InteractionState.GetReplicationKey())
	{
		AActor* FocusedActor = InteractionState.GetFocusedActor();
		UVetInteractiveComponent* CurrentInteractive = !IsValid(FocusedActor) ? nullptr : IVetInteractiveInterface::GetInteractiveComponent_Internal(FocusedActor);

		if (InteractionState.IsInteracting())
		{
			OnInteractionStarted.Broadcast(CurrentInteractive);
			ConditionallySetTickEnabled(/*bInEnabled =*/ false);
		}
		else
		{
			OnInteractionEnded.Broadcast(CurrentInteractive, InteractionState.GetResult());
			ConditionallySetTickEnabled(/*bInEnabled =*/ true);
		}
	}

	if (InteractionState.GetFocusedComponent() != InPreviousState.GetFocusedComponent())
	{
		SwitchFocusedComponent(InteractionState.GetFocusedComponent(), InPreviousState.GetFocusedComponent());
	}
}

bool UVetInteractionComponent::IsLocal() const
{

	APlayerController* const PC = GetWorld()->GetFirstPlayerController();
	return IsValid(PC) && PC->IsLocalController();
}

void UVetInteractionComponent::PrintDebugMessage(int32 InKey, const FString& InDebugMessage, float InTimeToDisplay /*= 10.0f*/)
{
#if WITH_EDITOR
	if (bShowDebugMessages)
	{
		GEngine->AddOnScreenDebugMessage(InKey, InTimeToDisplay, FColor::Red, InDebugMessage);
	}
#endif
}

bool UVetInteractionComponent::CheckConstructorContext(const TCHAR* InContext) const
{
	check(InContext != nullptr && "Check constructor context called without a context!");
	return ensureAlwaysMsgf(FUObjectThreadContext::Get().IsInConstructor, TEXT("Default Initializer function %s can only be called from within the actor constructor!"), InContext);
}

void UVetInteractionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UVetInteractionComponent, InteractionState);
}