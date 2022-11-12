
package com.example.h264_relay_server;

import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.Gravity;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.TextView;
import android.content.Context;
import android.content.res.AssetManager;

public class H264RelayServerTest extends AppCompatActivity {

    private static final String TAG = "H264RelayServerTest";

    private static H264RelayServerJni server_jni_;

    private RadioGroup select_res_rg_;
    private String select_res_str_;
    private Button start_server_btn_;
    private TextView url_tv_;

    private Handler ui_handler_ = new Handler();
    private Handler restart_server_handler_;
    private HandlerThread restart_server_thread_;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        //setContentView(R.layout.activity_main);
        setContentView(getWigetView(this));

        select_res_rg_.setOnCheckedChangeListener(new RadioGroup.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(RadioGroup group, int checkedId) {
                if (checkedId == 1) {
                    select_res_str_ = "wildlife.1m.h264";
                }
                else {
                    select_res_str_ = "outdoors.5m.h264";
                }
            }
        });

        start_server_btn_.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {

                Log.d(TAG, "start_server_btn_: onClick()");
                // james: test.
                AssetManager assetManager = getAssets();
                server_jni_.EnableUnittest(assetManager, select_res_str_);

                restart_server_thread_ = new HandlerThread("Restart Server Thread");
                restart_server_thread_.start();
                restart_server_handler_ = new Handler(restart_server_thread_.getLooper());
                restart_server_handler_.post(r1);

                select_res_rg_.setVisibility(View.GONE);
                start_server_btn_.setVisibility(View.GONE);
            }
        });

        server_jni_.setOnEventListener(new H264RelayServerJni.OnEventListener() {
            @Override
            public void OnStatus(String addr, int status) {
                Log.d(TAG, "OnStatus: addr=" + addr + ", status=" + status);
            }
        });

        start_server_btn_.performClick();
    }

    private Runnable r1 = new Runnable () {
        public void run() {

            boolean res = true;
            res = server_jni_.CreateServer();

            if (true != res) {
                Log.e(TAG, "CreateServer error...");
            }
        }
    };

    public View getWigetView(final Context context) {
        LayoutParams fill_layout_params = new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
        LayoutParams wrap_layout_params = new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);

        LinearLayout linear_layout = new LinearLayout(context);
        linear_layout.setOrientation(LinearLayout.VERTICAL);
        linear_layout.setLayoutParams(fill_layout_params);
        linear_layout.setGravity(Gravity.CENTER);

        select_res_rg_ = new RadioGroup(context);
        select_res_rg_.setGravity(Gravity.CENTER);
        //select_res_rg_.setOrientation(LinearLayout.HORIZONTAL);
        RadioButton rb1 = new RadioButton(context);
        rb1.setId(1);
        rb1.setText("720P_1Mbps");
        rb1.setChecked(false);
        select_res_rg_.addView(rb1);

        RadioButton rb2 = new RadioButton(context);
        rb2.setId(2);
        rb2.setText("1080P_5Mbps");
        rb2.setChecked(true);
        select_res_rg_.addView(rb2);

        select_res_str_ = "outdoors.5m.h264";

        start_server_btn_ = new Button(context);
        start_server_btn_.setText("start server");
        start_server_btn_.setGravity(Gravity.CENTER);

        linear_layout.addView(select_res_rg_);
        linear_layout.addView(start_server_btn_);

        return linear_layout;
    }
}
