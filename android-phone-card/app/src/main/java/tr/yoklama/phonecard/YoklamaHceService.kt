package tr.yoklama.phonecard

import android.nfc.cardemulation.HostApduService
import android.os.Bundle

class YoklamaHceService : HostApduService() {
    override fun processCommandApdu(commandApdu: ByteArray?, extras: Bundle?): ByteArray {
        if (commandApdu == null) return STATUS_NOT_SUPPORTED
        if (commandApdu.contentEquals(SELECT_YOKLAMA_AID)) {
            val token = CredentialStore.token(this)
            // İlk atamada henüz token yoktur. Yüklemeye uygulamadan açıkça
            // izin verildiyse ESP32'nin ardından göndereceği 80 10 komutunu
            // kabul edebilmek için SELECT'e başarı cevabı dönmeliyiz.
            if (token == null) return if (CredentialStore.isProvisioningArmed(this)) STATUS_OK else STATUS_NOT_FOUND
            return ("YOK:T:$token").toByteArray(Charsets.US_ASCII) + STATUS_OK
        }
        if (commandApdu.size < 5 || commandApdu[0] != 0x80.toByte() || commandApdu[1] != 0x10.toByte()) return STATUS_NOT_SUPPORTED
        val length = commandApdu[4].toInt() and 0xFF
        if (commandApdu.size != length + 5) return STATUS_WRONG_DATA
        val parts = commandApdu.copyOfRange(5, commandApdu.size).toString(Charsets.UTF_8).split("|", limit = 3)
        if (parts.size != 3 || !parts[0].matches(Regex("[0-9A-F]{12}")) || !parts[1].matches(Regex("[A-Za-z0-9_-]{2,32}")) || !parts[2].matches(Regex("[^;|\\n\\r]{2,48}"))) return STATUS_WRONG_DATA
        return if (CredentialStore.provisionOnce(this, parts[0], parts[1], parts[2])) STATUS_OK else STATUS_NOT_FOUND
    }

    override fun onDeactivated(reason: Int) = Unit

    private companion object {
        val SELECT_YOKLAMA_AID = byteArrayOf(0x00, 0xA4.toByte(), 0x04, 0x00, 0x06, 0xF0.toByte(), 0x59, 0x4F, 0x4B, 0x4C, 0x41, 0x00)
        val STATUS_OK = byteArrayOf(0x90.toByte(), 0x00)
        val STATUS_NOT_FOUND = byteArrayOf(0x6A, 0x82.toByte())
        val STATUS_WRONG_DATA = byteArrayOf(0x6A, 0x80.toByte())
        val STATUS_NOT_SUPPORTED = byteArrayOf(0x6D, 0x00)
    }
}
