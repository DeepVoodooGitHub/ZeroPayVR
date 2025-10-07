#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ZeroPay_BodySocket_Interface_r1.generated.h"

// Forward declare your socket component
class UZeroPay_BodySocket_r1;

UINTERFACE(BlueprintType)
class ZEROPAYMOD_API UZeroPay_BodySocket_Interface_r1 : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for any object that can decide if it wants to be socketed into a UZeroPay_BodySocket_r1
 */
class ZEROPAYMOD_API IZeroPay_BodySocket_Interface_r1
{
	GENERATED_BODY()

public:

	/**
	 * Return true if this object wants to be socketed into the specified body socket.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "ZeroPay|Sockets")
	bool WantsToBeSocketed(UZeroPay_BodySocket_r1* BodySocket);

	/* Called when the body sockets allows us to be socketed to it, this MAY occur 
	   multiple times with a unregister if your colliders are large and the body sockets
	   small (i.e. you overlap multiple sockets at once) */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "ZeroPay|Sockets")
	void RegisterSocket(UZeroPay_BodySocket_r1* BodySocket);

	/* Called when the body socket want's to unregister with you */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "ZeroPay|Sockets")
	void UnregisterSocket(UZeroPay_BodySocket_r1* BodySocket);


};
