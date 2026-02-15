package com.voxowl.tools;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.ComponentName;
import android.content.res.AssetManager;
import android.net.Uri;
import android.util.Log;
import androidx.core.content.FileProvider;

import com.voxowl.blip.R;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;

public class Filesystem {

    public static final int IMPORT_REQUEST_CODE = 1;

    /**
     *
     * @param ctx
     */
    static public void init(Context ctx, Activity activity) {
        Filesystem.applicationContext = ctx;
        Filesystem.assetManager = ctx.getAssets();
        Filesystem.activity = activity;
    }

    /**
     *
     */
    static public AssetManager getAssetManager() {
        return Filesystem.assetManager;
    }

    /**
     * Opens a file picker
     */
    static public void importFile() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        // TODO: gaetan: we should allow the app to specify the type of files (MIME)
        intent.setType("*/*"); // "application/*" "image/*"

        // request a file browser, result is captured in MainActivity.onActivityResult
        Filesystem.activity.startActivityForResult(Intent.createChooser(intent, "Cubzh"), IMPORT_REQUEST_CODE);
    }

    /**
     * Opens a share panel for a file
     */
    static public void shareFile(String filename, int type) {
        // construct Uri for image file
        String storageDir = Filesystem.applicationContext.getExternalFilesDir(null) + "/";
        String filepath = storageDir + filename;
        File file = new File(filepath);
        Uri uriToFile = FileProvider.getUriForFile(
                Filesystem.applicationContext,
                "com.voxowl.blip.provider",
                file);

        // open Android share sheet
        Intent shareIntent = new Intent();
        shareIntent.setAction(Intent.ACTION_SEND);
        shareIntent.setFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        if (type == 0) { // FileType::PNG
            shareIntent.setType("image/png");
        } else {
            shareIntent.setType("application/octet-stream");
        }
        shareIntent.putExtra(Intent.EXTRA_STREAM, uriToFile);

        // TODO: change text resource used here
        Filesystem.activity.startActivity(Intent.createChooser(shareIntent, Filesystem.applicationContext.getResources().getText(R.string.app_name)));
    }



    // --------------------------------------------------
    //
    // PRIVATE
    //
    // --------------------------------------------------

    /**
     *
     */
    static private Context applicationContext = null;

    /**
     *
     */
    static private AssetManager assetManager = null;

    /**
     *
     */
    static private Activity activity = null;

    /**
     *
     * @param assets
     * @param directoryPath
     * @return
     */
    static private ArrayList<String> deepListFiles(AssetManager assets, String directoryPath) {
        ArrayList<String> result = new ArrayList<String>();
        try {
            String[] filenames = assets.list(directoryPath);
            for (String filename: filenames) {
                String fullpath = directoryPath + "/" + filename;
                try {
                    InputStream is = assets.open(fullpath);
                    result.add(fullpath);
                } catch (IOException e) {
                    result.addAll(deepListFiles(assets, fullpath));
                }
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
        return result;
    }

    /**
     *
     * @param in
     * @param out
     * @throws IOException
     */
    static private void copyFile(InputStream in, OutputStream out) throws IOException {
        byte[] buffer = new byte[1024];
        int read;
        while((read = in.read(buffer)) != -1){
            out.write(buffer, 0, read);
        }
    }

}
