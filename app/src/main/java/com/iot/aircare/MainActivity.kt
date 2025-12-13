package com.iot.aircare

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.google.firebase.database.DataSnapshot
import com.google.firebase.database.DatabaseError
import com.google.firebase.database.FirebaseDatabase
import com.google.firebase.database.ValueEventListener
import com.iot.aircare.ui.theme.AirCareTheme

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        setContent {
            AirCareTheme {
                AirCareScreen()
            }
        }
    }
}

@Composable
fun AirCareScreen() {
    // State yang terikat ke UI
    var status by remember { mutableStateOf("—") }
    var sensorValue by remember { mutableIntStateOf(0) }
    var baseline by remember { mutableIntStateOf(0) }
    var lastUpdated by remember { mutableStateOf("—") }

    // Pemetaan warna berdasarkan status
    fun statusColor(s: String): Color = when (s.uppercase()) {
        "AMAN" -> Color(0xFF4CAF50)      // hijau
        "PERINGATAN" -> Color(0xFFFFEB3B) // kuning
        "BAHAYA" -> Color(0xFFF44336)     // merah
        else -> Color.LightGray
    }

    // remember dbRef supaya tidak dibuat ulang tiap recomposition
    val dbRef = remember {
        FirebaseDatabase.getInstance().getReference("aircare")
    }

    // Pasang listener ke Firebase Realtime Database
    DisposableEffect(Unit) {
        val statusListener = object : ValueEventListener {
            override fun onDataChange(snapshot: DataSnapshot) {
                val s = snapshot.getValue(String::class.java)
                if (s != null) status = s
            }
            override fun onCancelled(error: DatabaseError) {}
        }

        val sensorListener = object : ValueEventListener {
            override fun onDataChange(snapshot: DataSnapshot) {
                val v = snapshot.getValue(Int::class.java)
                    ?: snapshot.getValue(Long::class.java)?.toInt()
                if (v != null) sensorValue = v
            }
            override fun onCancelled(error: DatabaseError) {}
        }

        val baselineListener = object : ValueEventListener {
            override fun onDataChange(snapshot: DataSnapshot) {
                val b = snapshot.getValue(Int::class.java)
                    ?: snapshot.getValue(Long::class.java)?.toInt()
                if (b != null) baseline = b
            }
            override fun onCancelled(error: DatabaseError) {}
        }

        val timeListener = object : ValueEventListener {
            override fun onDataChange(snapshot: DataSnapshot) {
                val t = snapshot.getValue(Long::class.java) ?: snapshot.getValue(Int::class.java)?.toLong()
                if (t != null) lastUpdated = "${t}s"
            }
            override fun onCancelled(error: DatabaseError) {}
        }

        dbRef.child("status").addValueEventListener(statusListener)
        dbRef.child("sensorValue").addValueEventListener(sensorListener)
        dbRef.child("baseline").addValueEventListener(baselineListener)
        dbRef.child("timestamp").addValueEventListener(timeListener)

        onDispose {
            dbRef.child("status").removeEventListener(statusListener)
            dbRef.child("sensorValue").removeEventListener(sensorListener)
            dbRef.child("baseline").removeEventListener(baselineListener)
            dbRef.child("timestamp").removeEventListener(timeListener)
        }
    }

    // UI (sama seperti kamu punya)
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp)
    ) {
        Text("AirCare — Realtime Monitoring", style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Bold)
        Spacer(modifier = Modifier.height(12.dp))

        // Card status
        Card(
            modifier = Modifier
                .fillMaxWidth()
                .height(120.dp),
            shape = RoundedCornerShape(12.dp)
        ) {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .background(statusColor(status))
                    .padding(16.dp),
                contentAlignment = Alignment.CenterStart
            ) {
                Column {
                    Text("Status", fontSize = 14.sp, color = Color.Black)
                    Spacer(modifier = Modifier.height(6.dp))
                    Text(status, fontSize = 28.sp, fontWeight = FontWeight.ExtraBold, color = Color.Black)
                    Spacer(modifier = Modifier.height(6.dp))
                    Text("Updated: $lastUpdated", fontSize = 12.sp, color = Color.Black)
                }
            }
        }

        Spacer(modifier = Modifier.height(12.dp))

        // Sensor value card
        Card(
            modifier = Modifier
                .fillMaxWidth()
                .height(110.dp),
            shape = RoundedCornerShape(12.dp)
        ) {
            Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.Center) {
                Text("Sensor (ADC)", fontSize = 14.sp)
                Spacer(modifier = Modifier.height(8.dp))
                Text(sensorValue.toString(), fontSize = 36.sp, fontWeight = FontWeight.Bold)
                Spacer(modifier = Modifier.height(6.dp))
                Text("Baseline: $baseline", fontSize = 12.sp)
            }
        }

        Spacer(modifier = Modifier.height(18.dp))

        // Tips / note
        Card(modifier = Modifier.fillMaxWidth(), shape = RoundedCornerShape(8.dp)) {
            Column(modifier = Modifier.padding(12.dp)) {
                Text("Catatan:", fontWeight = FontWeight.SemiBold)
                Spacer(modifier = Modifier.height(6.dp))
                Text("- Pastikan ESP32 terhubung ke WiFi dan kirim data ke /aircare")
                Spacer(modifier = Modifier.height(4.dp))
                Text("- Untuk produksi, perbaiki security rules Realtime Database")
            }
        }
    }
}