package com.voxowl.tools

import android.os.Build
import android.util.Log
import androidx.credentials.CreatePublicKeyCredentialRequest
import androidx.credentials.CreatePublicKeyCredentialResponse
import androidx.credentials.GetCredentialRequest
import androidx.credentials.GetPublicKeyCredentialOption
import androidx.credentials.PublicKeyCredential
import com.voxowl.blip.GameApplication
import com.voxowl.blip.MainActivity
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.serialization.Serializable
import kotlinx.serialization.json.Json
import kotlinx.serialization.encodeToString
import java.util.Base64

final class Passkey {

    companion object {

        // store a pretty JSON encoder/decoder
        private val jsonPretty = Json { prettyPrint = true }

        @JvmStatic
        fun isAvailable(): Boolean {
            // Passkeys are supported on Android 9 (API level 28) and above
            return Build.VERSION.SDK_INT >= Build.VERSION_CODES.P
        }

        @JvmStatic
        fun createPasskey(relyingPartyIdentifier: String,
                          challenge: String,
                          userID: String,
                          username: String): String? {

            Log.d("Blip", "🔑[Java][createPasskey] $relyingPartyIdentifier $username $userID $challenge")
            
            // ensure passkeys are available on this device
            if (!isAvailable()) {
                return "Passkeys are not supported on this device"
            }

            // validate arguments
            if (relyingPartyIdentifier.isEmpty()) {
                return "Relying party identifier is required"
            }
            if (challenge.isEmpty()) {
                return "Challenge is required"
            }
            if (userID.isEmpty()) {
                return "User ID is required"
            }
            if (username.isEmpty()) {
                return "Username is required"
            }

            // create a minimal request JSON string
            val creationOptions = PublicKeyCredentialCreationOptionsJSON(
                rp = PublicKeyCredentialRpEntity(
                    id = relyingPartyIdentifier,
                    name = "Blip",
                ),
                user = PublicKeyCredentialUserEntityJSON(
                    id = userID.toBase64URLString(),
                    name = username,
                    displayName = username
                ),
                challenge = challenge.toBase64URLString(),
                pubKeyCredParams = listOf(
                    PublicKeyCredentialParameters(
                        type = "public-key",
                        alg = COSEAlgorithmIdentifier_ES256,
                    ),
                ),
                authenticatorSelection = AuthenticatorSelectionCriteria(
                    authenticatorAttachment = "platform",
                    requireResidentKey = true, // false
                    residentKey = "required", // preferred
                    userVerification = "required" // preferred
                ),
                // timeout = 60000, // 60 seconds
                // excludeCredentials = emptyList(),
                // attestation = "none"
            )
            val requestJson = Json.encodeToString(creationOptions)
            
            // Let's also log the JSON in a pretty format for debugging
            try {
                val prettyJson = jsonPretty.encodeToString(creationOptions)
                Log.d("Blip", "🔑 CREATE PASSKEY SIMPLE JSON (pretty):\n$prettyJson")
            } catch (e: Exception) {
                Log.e("Blip", "🔑 Failed to create pretty JSON", e)
            }

            val credentialManager = GameApplication.shared().credentialManager
            val coroutineScope = CoroutineScope(Dispatchers.IO)

            coroutineScope.launch {
                try {
                    Log.d("Blip", "🔑 Creating simple CreatePublicKeyCredentialRequest...")
                    val registrationRequest = CreatePublicKeyCredentialRequest(
                        requestJson = requestJson,
                        preferImmediatelyAvailableCredentials = false
                    )
                    Log.d("Blip", "🔑 Simple CreatePublicKeyCredentialRequest created successfully")

                    val activity = MainActivity.shared
                    if (activity == null) {
                        Log.e("Blip", "🔑 MainActivity is null, cannot create passkey")
                        onPasskeyCreationFailed("MainActivity is null")
                        return@launch
                    }

                    Log.d("Blip", "🔑 Calling credentialManager.createCredential (simple)...")
                    val result = credentialManager.createCredential(
                        activity,
                        registrationRequest
                    ) as CreatePublicKeyCredentialResponse

                    // Send the attestation response to your backend
                    val attestationResponseJson = result.registrationResponseJson
                    Log.d("Blip", "🔑 Simple passkey created successfully: $attestationResponseJson")
                    
                    // Call success callback
                    onPasskeyCreated(attestationResponseJson)

                } catch (e: Exception) {
                    Log.e("Blip", "🔑 Failed to create simple passkey", e)
                    Log.e("Blip", "🔑 Exception type: ${e.javaClass.simpleName}")
                    Log.e("Blip", "🔑 Exception message: ${e.message}")
                    
                    // Call failure callback
                    val errorMessage = "${e.javaClass.simpleName}: ${e.message}"
                    onPasskeyCreationFailed(errorMessage)
                }
            }

            return null // Return null to indicate success
        }

        @JvmStatic
        fun loginWithPasskey(relyingPartyIdentifier: String, challengeBytes: String): String? {
            
            Log.d("Blip", "🔑[Java][loginWithPasskey] $relyingPartyIdentifier $challengeBytes")
            
            // ensure passkeys are available on this device
            if (!isAvailable()) {
                return "Passkeys are not supported on this device"
            }

            // validate arguments
            if (relyingPartyIdentifier.isEmpty()) {
                return "Relying party identifier is required"
            }
            if (challengeBytes.isEmpty()) {
                return "Challenge is required"
            }

            // create authentication request JSON string
            val requestOptions = PublicKeyCredentialRequestOptionsJSON(
                challenge = challengeBytes.toBase64URLString(),
                rpId = relyingPartyIdentifier,
                userVerification = "preferred", // can be "required", "preferred", or "discouraged"
                timeout = 60000 // 60 seconds
            )
            val requestJson = Json.encodeToString(requestOptions)
            
            // Let's also log the JSON in a pretty format for debugging
            try {
                val prettyJson = jsonPretty.encodeToString(requestOptions)
                Log.d("Blip", "🔑 LOGIN WITH PASSKEY JSON (pretty):\n$prettyJson")
            } catch (e: Exception) {
                Log.e("Blip", "🔑 Failed to create pretty JSON for login", e)
            }

            val credentialManager = GameApplication.shared().credentialManager
            val coroutineScope = CoroutineScope(Dispatchers.IO)

            coroutineScope.launch {
                try {
                    Log.d("Blip", "🔑 Creating GetPublicKeyCredentialOption...")
                    val getCredOption = GetPublicKeyCredentialOption(
                        requestJson = requestJson
                    )

                    val getCredRequest = GetCredentialRequest.Builder()
                        .addCredentialOption(getCredOption)
                        .build()

                    val activity = MainActivity.shared
                    if (activity == null) {
                        Log.e("Blip", "🔑 MainActivity is null, cannot login with passkey")
                        onPasskeyLoginFailed("MainActivity is null")
                        return@launch
                    }

                    Log.d("Blip", "🔑 Calling credentialManager.getCredential...")
                    val result = credentialManager.getCredential(
                        activity,
                        getCredRequest
                    )

                    // Handle the authentication result
                    when (val credential = result.credential) {
                        is PublicKeyCredential -> {
                            val authenticationResponseJson = credential.authenticationResponseJson
                            Log.d("Blip", "🔑 Passkey login successful: $authenticationResponseJson")
                            
                            // Call success callback
                            onPasskeyLoginSucceeded(authenticationResponseJson)
                        }
                        else -> {
                            Log.e("Blip", "🔑 Unexpected credential type: ${credential.javaClass.simpleName}")
                            onPasskeyLoginFailed("Unexpected credential type")
                        }
                    }
                } catch (e: Exception) {
                    Log.e("Blip", "🔑 Unexpected error during passkey login", e)
                    Log.e("Blip", "🔑 Exception type: ${e.javaClass.simpleName}")
                    Log.e("Blip", "🔑 Exception message: ${e.message}")
                    
                    // Call failure callback
                    val errorMessage = "${e.javaClass.simpleName}: ${e.message}"
                    onPasskeyLoginFailed(errorMessage)
                }
            }

            return null
        }

        // Native callback functions that will be called from Kotlin to notify C++
        @JvmStatic
        external fun onPasskeyCreated(attestationResponseJson: String)

        @JvmStatic
        external fun onPasskeyCreationFailed(errorMessage: String)

        @JvmStatic
        external fun onPasskeyLoginSucceeded(authenticationResponseJson: String)

        @JvmStatic
        external fun onPasskeyLoginFailed(errorMessage: String)
    }
}

// Base64URLString
typealias Base64URLString = String

// Convert data (bytes) to a Base64URLString
fun ByteArray.toBase64URLString(): Base64URLString {
     return Base64.getUrlEncoder().withoutPadding().encodeToString(this)
}

// Convert a String to a Base64URLString
fun String.toBase64URLString(): Base64URLString {
    return Base64.getUrlEncoder().withoutPadding().encodeToString(this.toByteArray())
}

// COSEAlgorithmIdentifier
typealias COSEAlgorithmIdentifier = Int
const val COSEAlgorithmIdentifier_ES256: COSEAlgorithmIdentifier = -7
// ...

@Serializable
data class PublicKeyCredentialRpEntity(
    val id: String,
    val name: String,
)

@Serializable
data class PublicKeyCredentialUserEntityJSON(
    val id: Base64URLString,
    val name: String,
    val displayName: String,
)

@Serializable
data class PublicKeyCredentialParameters(
    val type: String, // "public-key"
    val alg: Int
)

@Serializable
data class PublicKeyCredentialCreationOptionsJSON(
    val rp: PublicKeyCredentialRpEntity,
    val user: PublicKeyCredentialUserEntityJSON,
    val challenge: Base64URLString,
    val pubKeyCredParams: List<PublicKeyCredentialParameters>,
    val timeout: Long? = null,
    val excludeCredentials: List<PublicKeyCredentialDescriptorJSON> = emptyList(),
    val authenticatorSelection: AuthenticatorSelectionCriteria? = null,
    // val hints: List<String> = emptyList(),
    val attestation: String = "none",
    // val attestationFormats: List<String> = emptyList(),
    // val extensions: AuthenticationExtensionsClientInputsJSON? = null
)

 @Serializable
 data class PublicKeyCredentialDescriptorJSON(
     val type: String = "public-key",
     val id: String,
     val transports: List<String>? = null
 )

 @Serializable
 data class AuthenticatorSelectionCriteria(
     val authenticatorAttachment: String? = null,
     val requireResidentKey: Boolean? = null, // TODO: gaetan: we might be able to remove this field (it's legacy)
     val residentKey: String? = null,
     val userVerification: String? = null
 )

@Serializable
data class PublicKeyCredentialRequestOptionsJSON(
    val challenge: Base64URLString,
    val timeout: Long? = null,
    val rpId: String? = null,
    val allowCredentials: List<PublicKeyCredentialDescriptorJSON> = emptyList(),
    val userVerification: String? = null,
    // val hints: List<String> = emptyList(),
    // val extensions: AuthenticationExtensionsClientInputsJSON? = null
)

// @Serializable
// data class AuthenticationExtensionsClientInputsJSON(
//     val credProps: Boolean? = null
// )

// @JvmStatic
// fun createPasskeyCOMPLEX(relyingPartyIdentifier: String,
//                   challenge: String,
//                   userID: String,
//                   username: String): String? {

//     Log.d("Blip 🔑", "CREATE PASSKEY $relyingPartyIdentifier $username $userID $challenge")

//     // TODO: gaetan: add a callback to the function!!!

//     // ensure passkeys are available on this device
//     if (!isAvailable()) {
//         return "Passkeys are not supported on this device"
//     }

//     // validate arguments
//     if (relyingPartyIdentifier.isEmpty()) {
//         return "Relying party identifier is required"
//     }
//     if (challenge.isEmpty()) {
//         return "Challenge is required"
//     }
//     if (userID.isEmpty()) {
//         return "User ID is required"
//     }
//     if (username.isEmpty()) {
//         return "Username is required"
//     }

//     // create the request JSON string
//     // TODO: gaetan: create a function to generate this JSON string
//     val creationOptions = PublicKeyCredentialCreationOptionsJSON(
//         rp = PublicKeyCredentialRpEntity(
//             id = relyingPartyIdentifier,
//             name = "Blip",
//         ),
//         user = PublicKeyCredentialUserEntityJSON(
//             id = userID.toBase64URLString(),
//             name = username,
//             displayName = username
//         ),
//         challenge = challenge.toBase64URLString(),
//         pubKeyCredParams = listOf(
//             PublicKeyCredentialParameters(
//                 type = "public-key",
//                 alg = COSEAlgorithmIdentifier_ES256,
//             ),
//         ),
//         authenticatorSelection = AuthenticatorSelectionCriteria(
//             authenticatorAttachment = "platform",
//             requireResidentKey = false, // Changed to false for better compatibility
//             residentKey = "preferred", // Changed from "required" to "preferred"
//             userVerification = "preferred", // Changed from "required" to "preferred"
//         ),
//     )
//     val requestJson = Json.encodeToString(creationOptions)
//     Log.d("Blip 🔑", "CREATE PASSKEY JSON: $requestJson")

//     val credentialManager = GameApplication.shared().credentialManager
//     val coroutineScope = CoroutineScope(Dispatchers.IO)

//     coroutineScope.launch {
//         try {
//             Log.d("Blip 🔑", "Creating CreatePublicKeyCredentialRequest...")
//             val registrationRequest = CreatePublicKeyCredentialRequest(
//                 requestJson = requestJson,
//                 preferImmediatelyAvailableCredentials = false
//             )
//             Log.d("Blip 🔑", "CreatePublicKeyCredentialRequest created successfully")

//             val activity = MainActivity.shared
//             if (activity == null) {
//                 Log.e("Blip 🔑", "MainActivity is null, cannot create passkey")
//                 return@launch
//             }

//             Log.d("Blip 🔑", "Calling credentialManager.createCredential...")
//             val result = credentialManager.createCredential(
//                 activity,
//                 registrationRequest
//             ) as CreatePublicKeyCredentialResponse

//             // Send the attestation response to your backend
//             val attestationResponseJson = result.registrationResponseJson
//             Log.d("Blip 🔑", "Passkey created successfully: $attestationResponseJson")
//             // TODO: Send attestationResponseJson to your backend

//         } catch (e: Exception) {
//             Log.e("Blip 🔑", "Failed to create passkey", e)
//             Log.e("Blip 🔑", "Exception type: ${e.javaClass.simpleName}")
//             Log.e("Blip 🔑", "Exception message: ${e.message}")
//             // TODO: Handle other exceptions and return appropriate error messages
//         }
//     }

//     return null // Return null to indicate success
// }