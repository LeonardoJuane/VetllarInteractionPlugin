// (c) 2022 Property of Leonardo F. Juane - All rights reserved


#include "InteractiveInterface.h"
#include "Components/InteractionComponent.h"
#include "Components/InteractiveComponent.h"

DEFINE_LOG_CATEGORY(LogVetInteractiveInterface);

EVetInteractability IVetInteractiveInterface::GetInteractabilityState_Internal(AActor* InInteractive)
{
	if (!IsValid(InInteractive))
	{
		return EVetInteractability::Unavailable;
	}

	if (UVetInteractiveComponent* const InteractiveComponent = GetInteractiveComponent_Internal(InInteractive))
	{
		//If internally we are unavailable, we don't even ask overrides about it.
		EVetInteractability IntaractabilityState = InteractiveComponent->GetInteractabilityState();
		if (IntaractabilityState < EVetInteractability::Unavailable)
		{
			//Will only be valid if the interface is implemented in C++.
			if (IVetInteractiveInterface* NativeInterface = Cast<IVetInteractiveInterface>(InInteractive))
			{
				const EVetInteractability NativeInteractabilty = NativeInterface->GetInteractabilityState();

				//Pick the least interactible of the states
				IntaractabilityState = NativeInteractabilty > IntaractabilityState ? NativeInteractabilty : IntaractabilityState;
			}

			bool bBPFunctionImplemented{ false };
			const EVetInteractability BP_Interactability = IVetInteractiveInterface::Execute_K2_GetDesiredInteractabilityState(InInteractive, bBPFunctionImplemented);

			//Pick the least interactible of the states if the blueprint function is implemented
			IntaractabilityState = (bBPFunctionImplemented && BP_Interactability > IntaractabilityState) ? BP_Interactability : IntaractabilityState;
		}
		return IntaractabilityState;
	}
	return EVetInteractability::Unavailable;
}

bool IVetInteractiveInterface::CanBeInteractedWith_Internal(AActor* InInteractive, UVetInteractionComponent* InInteractor)
{
	if (!IsValid(InInteractive) || !IsValid(InInteractor))
	{
		return false;
	}

	const EVetInteractability InteractabilityState = GetInteractabilityState_Internal(InInteractive);
	if (InteractabilityState != EVetInteractability::Available)
	{
		return false;
	}

	if (UVetInteractiveComponent* const InteractiveComponent = GetInteractiveComponent_Internal(InInteractive))
	{
		//Internal check
		if (!InteractiveComponent->CanBeInteractedWith(*InInteractor))
		{
			return false;
		}

		//BP check
		bool bBP_CallImplemented{false};
		const bool bBP_CanBeInteractedWith = IVetInteractiveInterface::Execute_K2_CanBeInteractedWith(InInteractive, InInteractor, bBP_CallImplemented);

		if (bBP_CallImplemented && !bBP_CanBeInteractedWith)
		{
			return false;
		}

		//Native check
		if (IVetInteractiveInterface* const NativeInterface = Cast<IVetInteractiveInterface>(InInteractive))
		{
			return NativeInterface->CanBeInteractedWith(*InInteractor);
		}
	} 
	else
	{
		return false;
	}
	return true;
}

bool IVetInteractiveInterface::CanBeFocusedOn_Internal(AActor* InInteractive, UVetInteractionComponent* InInteractor)
{
	if (!IsValid(InInteractive) || !IsValid(InInteractor))
	{
		return false;
	}

	const EVetInteractability InteractabilityState = GetInteractabilityState_Internal(InInteractive);
	if (InteractabilityState == EVetInteractability::Unavailable)
	{
		return false;
	}

	if (UVetInteractiveComponent* const InteractiveComponent = GetInteractiveComponent_Internal(InInteractive))
	{
		//Internal check
		if (!InteractiveComponent->CanBeFocusedOn(*InInteractor))
		{
			return false;
		}

		//BP check
		bool bBP_CallImplemented{ false };
		const bool bBP_CanBeFocusedOn = IVetInteractiveInterface::Execute_K2_CanBeFocusedOn(InInteractive, InInteractor, bBP_CallImplemented);

		if (bBP_CallImplemented && !bBP_CanBeFocusedOn)
		{
			return false;
		}

		//Native check
		if (IVetInteractiveInterface* const NativeInterface = Cast<IVetInteractiveInterface>(InInteractive))
		{
			return NativeInterface->CanBeFocusedOn(*InInteractor);
		}
	}
	else
	{
		return false;
	}
	return true;
}

UVetInteractiveComponent* IVetInteractiveInterface::GetInteractiveComponent_Internal(AActor* InInteractive)
{
	if (!IsValid(InInteractive))
	{
		return nullptr;
	}

	//Try to grab the interactive component from fastest to slowest calls in case the end user forgot to implement the functions.
	UVetInteractiveComponent* InteractiveComponent{nullptr};

	//Check if blueprint implemented the call
	InteractiveComponent = IVetInteractiveInterface::Execute_K2_GetInteractiveComponent(InInteractive);
	if (IsValid(InteractiveComponent))
	{
		return InteractiveComponent;
	}

	//check if it is natively implemented
	if (IVetInteractiveInterface* const NativeInterface = Cast<IVetInteractiveInterface>(InInteractive))
	{
		InteractiveComponent = NativeInterface->GetInteractiveComponent();
	}

	if (!IsValid(InteractiveComponent))
	{
#if WITH_EDITOR
		UE_LOG(LogVetInteractiveInterface, Warning, TEXT("Actor %s does not implement either blueprint nor native GetInteractiveComponent methods. Attempting slow get..."), *InInteractive->GetName());
#endif //WITH_EDITOR
		
		InteractiveComponent = InInteractive->GetComponentByClass<UVetInteractiveComponent>();
	}

#if WITH_EDITOR
	if (!IsValid(InteractiveComponent))
	{
		UE_LOG(LogVetInteractiveInterface, Error, TEXT("Actor %s does not have an interactive component!"), *InInteractive->GetName());
	}
#endif //WITH_EDITOR
	return InteractiveComponent;
}
