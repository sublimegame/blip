package com.voxowl.blip

import android.content.Context
import android.text.BoringLayout
import android.util.Log
import com.android.installreferrer.api.InstallReferrerClient
import com.android.installreferrer.api.InstallReferrerStateListener
import com.android.installreferrer.api.ReferrerDetails
import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable
import kotlinx.serialization.encodeToString
import kotlinx.serialization.json.Json
import java.io.BufferedReader
import java.io.InputStreamReader
import java.io.OutputStreamWriter

final class InstallReferrerUtil {

    @Serializable
    data class InstallReferrerInfoGooglePlay(
        @SerialName("origin") val origin: String,
        @SerialName("referrerUrl") val referrerUrl: String,
        @SerialName("referrerClickTime") val referrerClickTime: Long,
        @SerialName("appInstallTime") val appInstallTime: Long,
        @SerialName("instantExperienceLaunched") val instantExperienceLaunched: Boolean,
    )

    // companion object is needed to define class methods and members
    companion object {

        // Constants
        private const val preferenceFilename: String = "install_referrer.txt"
        private const val preferenceKeyGooglePlayStore: String = "google_play_store"

        // Install Referer
        private lateinit var referrerClient: InstallReferrerClient

        // JNI function
        @JvmStatic
        external fun didReceiveInstallReferrerInfo(jsonString: String) : Boolean

        //
        fun sendInstallReferrerInfoToCppIfNeeded() {
            applicationContext?.let { ctx ->
                val alreadySent: Boolean = readPreferences(ctx, preferenceKeyGooglePlayStore)
                if (!alreadySent) {
                    // Google Play Install Referrer
                    referrerClient = InstallReferrerClient.newBuilder(ctx).build()
                    referrerClient.startConnection(object : InstallReferrerStateListener {

                        override fun onInstallReferrerSetupFinished(responseCode: Int) {
                            when (responseCode) {
                                InstallReferrerClient.InstallReferrerResponse.OK -> {
                                    // Connection established.
                                    Log.d("Cubzh Java", "[INSTALL REFERRER] Connection established.")
                                    val response: ReferrerDetails = referrerClient.installReferrer

                                    val referrerUrl: String = response.installReferrer
                                    val referrerClickTime: Long = response.referrerClickTimestampSeconds
                                    val appInstallTime: Long = response.installBeginTimestampSeconds
                                    val instantExperienceLaunched: Boolean = response.googlePlayInstantParam

                                    // Create an instance of the data class
                                    val info = InstallReferrerInfoGooglePlay("google-play", referrerUrl, referrerClickTime, appInstallTime, instantExperienceLaunched)

                                    // Convert the data class instance to a JSON string
                                    val jsonString = Json.encodeToString(info)

                                    // send JSON string to C++
                                    val ok = didReceiveInstallReferrerInfo(jsonString)
                                    if (ok) {
                                        savePreferences(ctx, preferenceKeyGooglePlayStore, true)
                                    }

                                    // disconnect from Google Play Store
                                    referrerClient.endConnection()
                                }
                                InstallReferrerClient.InstallReferrerResponse.FEATURE_NOT_SUPPORTED -> {
                                    // API not available on the current Play Store app.
                                    Log.d("Cubzh Java", "[INSTALL REFERRER] API not available on the current Play Store app.")
                                    referrerClient.endConnection()
                                }
                                InstallReferrerClient.InstallReferrerResponse.SERVICE_UNAVAILABLE -> {
                                    // Connection couldn't be established.
                                    Log.d("Cubzh Java", "[INSTALL REFERRER] Connection couldn't be established.")
                                    referrerClient.endConnection()
                                }
                            }
                        }

                        override fun onInstallReferrerServiceDisconnected() {
                            // Try to restart the connection on the next request to
                            // Google Play by calling the startConnection() method.
                            Log.d("Cubzh Java", "[INSTALL REFERRER] onInstallReferrerServiceDisconnected.")
                        }

                    })
                }
            }
        }

        // Writing preferences to internal storage
        private fun savePreferences(context: Context, key: String, value: Boolean) {
            try {
                val fileOutputStream = context.openFileOutput(preferenceFilename, Context.MODE_PRIVATE)
                val outputStreamWriter = OutputStreamWriter(fileOutputStream)
                outputStreamWriter.write("$key=$value")
                outputStreamWriter.close()
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }

        // Reading preferences from internal storage
        private fun readPreferences(context: Context, key: String): Boolean {
            try {
                val fileInputStream = context.openFileInput(preferenceFilename)
                val inputStreamReader = InputStreamReader(fileInputStream)
                val bufferedReader = BufferedReader(inputStreamReader)
                val line = bufferedReader.readLine()
                bufferedReader.close()

                // Parse the line to get the boolean value associated with the key
                val keyValue = line?.split("=")
                if (keyValue?.size == 2 && keyValue[0] == key) {
                    return keyValue[1].toBoolean()
                }
            } catch (e: Exception) {
                // will return false
            }

            // Default value if something goes wrong or if the key is not found
            return false
        }

        fun setContext(ctx: Context, activity: MainActivity) {
            this.applicationContext = ctx
            this.activity = activity
        }

        // --------------------------------------------------
        //
        // Local fields
        //
        // --------------------------------------------------
        private var applicationContext: Context? = null
        private var activity: MainActivity? = null

    } // companion object

}
