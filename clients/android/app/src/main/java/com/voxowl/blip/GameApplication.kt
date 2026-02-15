package com.voxowl.blip

import android.app.Application
import android.content.res.AssetManager
import android.content.res.Resources
import android.util.Log
import androidx.appcompat.widget.VectorEnabledTintResources
import androidx.credentials.CredentialManager

class GameApplication : Application() {

    // This is equivalent of static fields
    companion object {
        private var sharedInstance: GameApplication? = null
        private var cppLibLoaded: Boolean

        fun shared(): GameApplication {
            return sharedInstance ?: throw IllegalStateException("GameApplication is not initialized")
        }

        fun getCPPLibLoaded(): Boolean {
            return cppLibLoaded
        }

        init {
            System.loadLibrary("vx-wrapper")
            cppLibLoaded = true
            Log.v("Cubzh Java", "🐞Native library loaded 🐞")
        }

        private external fun init(assets: AssetManager, appStoragePath: String)
    }

    var newPushTokenReceived: String? = null
    lateinit var credentialManager: CredentialManager

    override fun onCreate() {
        super.onCreate()
        Log.v("Cubzh Java", "[GameApplication] [onCreate]")

        // Initialize the shared instance
        sharedInstance = this

        // Create Credential Manager
        // Use your app or activity context to instantiate a client instance of CredentialManager.
        this.credentialManager = CredentialManager.create(this)

        // Call VXApplication::init
        init(
            this.resources.assets,
            this.applicationContext.getExternalFilesDir(null)?.path.toString()
        )
    }

}