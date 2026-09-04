package tr.yoklama.phonecard

import android.content.Context

object CredentialStore {
    private const val PREFS = "yoklama_phone_card"
    private const val TOKEN = "credential_token"
    private const val STUDENT_NUMBER = "student_number"
    private const val STUDENT_NAME = "student_name"
    private const val PROVISIONING_ARMED = "provisioning_armed"

    private fun prefs(context: Context) = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
    fun token(context: Context): String? = prefs(context).getString(TOKEN, null)
    fun studentNumber(context: Context): String? = prefs(context).getString(STUDENT_NUMBER, null)
    fun studentName(context: Context): String? = prefs(context).getString(STUDENT_NAME, null)
    fun isProvisioningArmed(context: Context) = prefs(context).getBoolean(PROVISIONING_ARMED, false)

    fun armProvisioning(context: Context) {
        if (token(context) == null) prefs(context).edit().putBoolean(PROVISIONING_ARMED, true).apply()
    }

    fun provisionOnce(context: Context, token: String, number: String, name: String): Boolean {
        if (token.length != 12 || this.token(context) != null || !isProvisioningArmed(context)) return false
        return prefs(context).edit()
            .putString(TOKEN, token)
            .putString(STUDENT_NUMBER, number)
            .putString(STUDENT_NAME, name)
            .putBoolean(PROVISIONING_ARMED, false)
            .commit()
    }
}
