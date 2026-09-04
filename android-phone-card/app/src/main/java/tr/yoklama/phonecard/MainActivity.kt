package tr.yoklama.phonecard

import android.app.Activity
import android.graphics.Color
import android.os.Bundle
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView

class MainActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) { super.onCreate(savedInstanceState); showScreen() }

    private fun showScreen() {
        val padding = (24 * resources.displayMetrics.density).toInt()
        val root = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(padding, padding, padding, padding) }
        root.addView(TextView(this).apply { text = "Yoklama Telefon Kartı"; textSize = 25f; setTextColor(Color.rgb(24, 34, 53)) })
        val info = TextView(this).apply { textSize = 16f; setPadding(0, padding / 2, 0, padding / 2) }
        root.addView(info)
        val token = CredentialStore.token(this)
        if (token != null) {
            info.text = "Bu telefon ${CredentialStore.studentName(this)} (${CredentialStore.studentNumber(this)}) için atanmıştır.\n\nNFC açıkken telefonu okuyucuya yaklaştırın. Atama değiştirilemez."
        } else {
            info.text = if (CredentialStore.isProvisioningArmed(this)) "Yükleme için hazır. Web panelinde yükleme modunu açıp telefonu okuyucuya yaklaştırın." else "Telefonu atamak için aşağıdaki düğmeye basın. Ardından yalnızca adminin açtığı ESP32 yükleme modu kimlik yazabilir."
            val arm = Button(this).apply { text = if (CredentialStore.isProvisioningArmed(this@MainActivity)) "Yükleme için hazır" else "Yüklemeye hazırla"; isEnabled = !CredentialStore.isProvisioningArmed(this@MainActivity) }
            arm.setOnClickListener { CredentialStore.armProvisioning(this@MainActivity); showScreen() }
            root.addView(arm)
        }
        setContentView(root)
    }
}
