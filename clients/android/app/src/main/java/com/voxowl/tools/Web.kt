package com.voxowl.tools

import android.content.Intent
import android.net.Uri
import android.util.Log
import android.widget.Toast
import com.voxowl.blip.MainActivity

class Web {

    companion object {

        @JvmStatic
        fun openModal(url: String?) {
            open(url)
        }

        @JvmStatic
        fun open(url: String?) {
            // returns if activity is null
            val activity: MainActivity = MainActivity.shared ?: return
            try {
                val i = Intent(Intent.ACTION_VIEW)
                i.setData(Uri.parse(url))
                activity.startActivity(i)
            } catch (e: Exception) {
                Toast.makeText(activity, "No application found to open link.", Toast.LENGTH_SHORT).show()
                Log.e("CUBZH", "Web.open failed", e)
            }
        }

    }
}