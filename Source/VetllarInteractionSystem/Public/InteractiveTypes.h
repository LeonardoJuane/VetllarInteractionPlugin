// (c) 2023 Leonardo F. Juane - To be used under Boost Software License 1.0

#pragma once

UENUM(BlueprintType)
enum class EVetInteractability : uint8
{
	Available,					//Can be focused on and interacted with
	FocusableButUnavailable,	//Can be focused on but not interacted with
	Unavailable					//Can't be focused on nor interacted with
};

UENUM(BlueprintType)
enum class EVetInteractionResult : uint8
{
	Success,
	Cancelled
};