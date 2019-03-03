package id.ilmudata.agusk.mysmartmobile

import android.os.Bundle
import android.support.design.widget.Snackbar
import android.support.v7.app.AppCompatActivity
import android.view.Menu
import android.view.MenuItem
import android.view.View
import android.widget.Toast

import kotlinx.android.synthetic.main.activity_main.*
import android.widget.Switch
import com.android.volley.AuthFailureError
import com.android.volley.Request
import com.android.volley.Response
import com.android.volley.toolbox.StringRequest
import com.android.volley.toolbox.Volley



class MainActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        setSupportActionBar(toolbar)

        fab.setOnClickListener { view ->
            Snackbar.make(view, "Replace with your own action", Snackbar.LENGTH_LONG)
                .setAction("Action", null).show()
        }

        val lamp1witch = findViewById(R.id.lamp1) as Switch
        lamp1witch.setOnCheckedChangeListener { buttonView, isChecked ->
            if(isChecked){
                //Toast.makeText(this,"Lamp 1 on",Toast.LENGTH_LONG).show()
                applyLamp(1)
            }else{
                //Toast.makeText(this,"Lamp 1 off",Toast.LENGTH_LONG).show()
                applyLamp(2)
            }
        }
        val lamp2witch = findViewById(R.id.lamp2) as Switch
        lamp2witch.setOnCheckedChangeListener { buttonView, isChecked ->
            if(isChecked){
                //Toast.makeText(this,"Lamp 2 on",Toast.LENGTH_LONG).show()
                applyLamp(3)
            }else{
                //Toast.makeText(this,"Lamp 2 off",Toast.LENGTH_LONG).show()
                applyLamp(4)
            }
        }
        val lamp3witch = findViewById(R.id.lamp3) as Switch
        lamp3witch.setOnCheckedChangeListener { buttonView, isChecked ->
            if(isChecked){
                //Toast.makeText(this,"Lamp 3 on",Toast.LENGTH_LONG).show()
                applyLamp(5)
            }else{
                //Toast.makeText(this,"Lamp 3 off",Toast.LENGTH_LONG).show()
                applyLamp(6)
            }
        }
    }

    fun applyLamp(cmd: Int){
        val queue = Volley.newRequestQueue(this@MainActivity)
        val url = "http://192.168.0.1/lamp"

        val stringRequest = object: StringRequest(Request.Method.POST, url,
            Response.Listener<String> { response ->
                // Display the first 500 characters of the response string.
                Toast.makeText(this,"Response: $response",Toast.LENGTH_LONG).show()

            },
            Response.ErrorListener { volleyError ->
                Toast.makeText(this,"$volleyError" ,Toast.LENGTH_LONG).show()
            }){
            @Throws(AuthFailureError::class)
            override fun getBody(): ByteArray {
                return "$cmd".toByteArray()
            }
        }

        queue.add(stringRequest)

    }


    fun pingESP32(view : View){
        val queue = Volley.newRequestQueue(this@MainActivity)
        val url = "http://192.168.0.1/ping"

        val stringRequest = StringRequest(Request.Method.GET, url,
            Response.Listener<String> { response ->                
                Toast.makeText(this,"Response: $response",Toast.LENGTH_LONG).show()

            },
            Response.ErrorListener { volleyError ->
                Toast.makeText(this,"$volleyError",Toast.LENGTH_LONG).show()
            })

        queue.add(stringRequest)
    }

    fun getLampStates(view : View){
        val queue = Volley.newRequestQueue(this@MainActivity)
        val url = "http://192.168.0.1/state"

        val stringRequest = StringRequest(Request.Method.GET, url,
            Response.Listener<String> { response ->                
                Toast.makeText(this,"Response: $response",Toast.LENGTH_LONG).show()

            },
            Response.ErrorListener { volleyError ->
                Toast.makeText(this,"$volleyError",Toast.LENGTH_LONG).show()
            })

        queue.add(stringRequest)
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        // Inflate the menu; this adds items to the action bar if it is present.
        menuInflater.inflate(R.menu.menu_main, menu)
        return true
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        return when (item.itemId) {
            R.id.action_settings -> true
            else -> super.onOptionsItemSelected(item)
        }
    }
}
