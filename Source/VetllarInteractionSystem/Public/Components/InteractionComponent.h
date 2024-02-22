// (c) 2023 Leonardo F. Juane - To be used under Boost Software License 1.0

#pragma once

//Engine
#include "Components/ActorComponent.h"

//Interaction
#include "InteractiveTypes.h"
#include "InteractionComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogInteraction, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFocusedActorChanged, AActor*, InFocusedActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractionStarted, UVetInteractiveComponent*, InInteractive);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteractionEnded, UVetInteractiveComponent*, InInteractive, EVetInteractionResult, InInteractionResult);

//Type of traces supported by the system
UENUM(BlueprintType)
enum class EVetInteractionTraceType : uint8
{
	SphereTrace_FromOwner,	//A multi sphere trace from the owning actor and forward (runs on server and client).
	LineTrace_FromCursor	//A single line trace from the local player cursor location (runs on client).
};

USTRUCT()
struct VETLLARINTERACTIONSYSTEM_API FVetInteractionComponentState
{
	GENERATED_BODY()

	FORCEINLINE void SetIsInteracting(bool bInNewIsInteracting)
	{
		bIsInteracting = bInNewIsInteracting;
		ReplicationKey++;
	}

	bool IsInteracting() const { return bIsInteracting; }

	FORCEINLINE void SetResult(EVetInteractionResult InResult)
	{
		Result = InResult;
	}

	EVetInteractionResult GetResult() const { return Result; }

	FORCEINLINE void SetFocusedActor(AActor* InActor)
	{
		if (InActor != FocusedActor)
		{
			FocusedActor = InActor;
		}
	}

	AActor* GetFocusedActor() const { return FocusedActor; }
	uint64 GetReplicationKey() const { return ReplicationKey; }

private:

	UPROPERTY()
	bool bIsInteracting{false};

	UPROPERTY()
	EVetInteractionResult Result{EVetInteractionResult::Success};

	UPROPERTY()
	TObjectPtr<AActor> FocusedActor;

	//Used to broadcast changes to the client even if they happened in the same frame on the server
	UPROPERTY()
	uint64 ReplicationKey{0};
};


UCLASS( ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent) )
class VETLLARINTERACTIONSYSTEM_API UVetInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:	

	UVetInteractionComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable)
	void StartInteraction();

	//Touch screen dedicated interaction.
	//This will work exactly as a Start Interaction unless the trace type is set to "From Cursor".
	UFUNCTION(BlueprintCallable)
	void StartTouchInteraction();

	UFUNCTION(BlueprintCallable)
	void StopInteraction();

	UFUNCTION(BlueprintCallable)
	AActor* GetFocusedActor() const { return InteractionState.GetFocusedActor(); }

	//Default initializers

	void SetDefaultTraceChannel(ECollisionChannel InTraceChannel);
	void SetDefaultTraceType(EVetInteractionTraceType InTraceType);
	void SetDefaultInteractionDistance(float InInteractionDistance);
	void SetDefaultInteractionRadius(float InInteractionRadius);

	UPROPERTY(BlueprintAssignable)
	FOnFocusedActorChanged OnFocusedActorChanged;

	UPROPERTY(BlueprintAssignable)
	FOnInteractionStarted OnInteractionStarted;

	UPROPERTY(BlueprintAssignable)
	FOnInteractionEnded OnInteractionEnded;

protected:

	virtual void BeginPlay() override;

	//Trace channel to use for interactives
	UPROPERTY(EditDefaultsOnly)
	TEnumAsByte<ECollisionChannel> TraceChannel{ECC_Visibility};

	//The type of trace to run in order to find potential interactive objects.
	UPROPERTY(EditDefaultsOnly)
	EVetInteractionTraceType TraceType{EVetInteractionTraceType::SphereTrace_FromOwner};

	//The distance of the trace to find potential interactive objects.
	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "TraceType == EVetInteractionTraceType::SphereTrace_FromOwner", EditConditionHides))
	float InteractionDistance{ 100.f };

	//The radius of the sphere that will be swept during the trace.
	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "TraceType == EVetInteractionTraceType::SphereTrace_FromOwner", EditConditionHides))
	float InteractionRadius{ 100.f };

	UPROPERTY(EditAnywhere, AdvancedDisplay)
	bool bShowDebugMessages{false};

private:

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_StartInteraction(AActor* InFocusedActor);

	UFUNCTION(Server, Reliable)
	void Server_StopInteraction();

	void SetFocusedActor(AActor* InNewFocusedActor);
	void SwitchFocusedActor(AActor* InNewFocusedActor, AActor* InPreviousFocusedActor);

	AActor* GetClosestActorFromArray(const TArray<AActor*>& InActorArray);

	//Executes when the interaction has successfully completed
	void OnInteractionCompleted(UVetInteractiveComponent& InInteractive);

	void OnInteractionEnded_Internal();

	void TraceForInteractives(bool bInFromTouch = false);
	void GetTraceHitForLocalPlayerCursor(FHitResult& OutResult, bool bInFromTouch = false) const;

	void ConditionallySetTickEnabled(bool bInEnabled);

	UFUNCTION()
	void OnRep_InteractionState(const FVetInteractionComponentState& InPreviousState);

	//checks if the actor that owns this component is in a local player world.
	bool IsLocal() const;

	void PrintDebugMessage(int32 InKey, const FString& InDebugMessage, float InTimeToDisplay = 10.0f);

	bool CheckConstructorContext(const TCHAR* InContext) const;

	UPROPERTY(ReplicatedUsing = OnRep_InteractionState)
	FVetInteractionComponentState InteractionState;
};
