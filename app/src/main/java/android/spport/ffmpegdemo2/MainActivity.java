package android.spport.ffmpegdemo2;

import androidx.appcompat.app.AppCompatActivity;

import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.spport.mylibrary2.AudioTrackStaticModeHelper;
import android.spport.mylibrary2.AudioTrackStreamHelper;
import android.spport.mylibrary2.Demo;
import android.util.Log;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;

public class MainActivity extends AppCompatActivity {


    private Demo demo;
    AudioTrackStaticModeHelper audioTrackHelper;
    AudioTrackStreamHelper audioTrackStreamHelper;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Example of a call to a native method
        TextView tv = findViewById(R.id.sample_text);

        checkPermission();

        demo = new Demo();
        tv.setText(demo.stringFromJNI());
        String folderurl= Environment.getExternalStorageDirectory().getPath();
        File externalFilesDir = getExternalFilesDir(null);
        Log.i("MainActivity", "externalFilesDir: "+externalFilesDir);

//        demo.decodeVideo(folderurl+"/input.mp4", externalFilesDir+"/output7.yuv");
//        demo.decodeVideo2(folderurl+"/input.mp4", externalFilesDir+"/output8.yuv");
        demo.decodeAudio(folderurl+"/input.mp4", externalFilesDir+"/audio.pcm");


//        initAudioTrackStaticMode(externalFilesDir);
        initAudioTrackStreamMode(externalFilesDir);

    }

    private void initAudioTrackStreamMode(File externalFilesDir) {
        audioTrackStreamHelper = new AudioTrackStreamHelper();

        audioTrackStreamHelper.initAudioTrackParams(externalFilesDir+"/audio.pcm");
        audioTrackStreamHelper.play();
    }

    private void initAudioTrackStaticMode(File externalFilesDir) {
        audioTrackHelper = new AudioTrackStaticModeHelper(this);

        Log.i("MainActivity","initAudioTrack");

        audioTrackHelper.initAudioTrackParams(externalFilesDir+"/audio.pcm");
        audioTrackHelper.initStaticBuff();
        audioTrackHelper.setAudioTrackStateChangeListener(new AudioTrackStaticModeHelper.AudioTrackStateChangeListener() {
            @Override
            public void onPrepread() {
                audioTrackHelper.play();
            }
        });
    }

    private void checkPermission() {
        PermissionCheckerUtil checker = new PermissionCheckerUtil(this);
        boolean isPermissionOK = Build.VERSION.SDK_INT < Build.VERSION_CODES.M || checker.checkPermission();
        if (!isPermissionOK) {
            Toast.makeText(this, "相机权限允许", Toast.LENGTH_SHORT).show();
            Log.d("MainActivity", "onCreate: 获取了权限 ");
        } else {
            Log.d("MainActivity", "onCreate: 无权限 ");
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if(audioTrackHelper!=null){
            audioTrackHelper.destroy();
        }
        if(audioTrackStreamHelper!=null){
            audioTrackStreamHelper.destroy();
        }
    }
}

