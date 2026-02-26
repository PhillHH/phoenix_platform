// ==========================================================================
// FILE: src/Services/LocalizationManager.cpp
// Phoenix v108.0 — Multi-language UI (DE/EN/FR/ES/IT/PT/NL/PL/TR)
// NVS-persisted language preference, string table lookup
// ==========================================================================
#include "phoenix/Core/Result.h"
#include "phoenix/Core/FixedString.h"
#include <esp_log.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <cstring>

namespace phoenix {

static const char* TAG = "i18n";

enum class Language : uint8_t {
    DE = 0, EN, FR, ES, IT, PT, NL, PL, TR,
    _COUNT
};

enum class StringID : uint16_t {
    // ── Navigation ──
    HOME = 0, TEST, RESULTS, CALIBRATE, PATIENTS, SETTINGS,
    AI_MODE, REPORTS, QC, EXPORT_BTN,
    // ── Measurement ──
    MEASURING, CANCEL, MEASUREMENT_COMPLETE, NO_PEAKS,
    CONTROL_WEAK, HIGH_BG, LOW_SNR, OUT_OF_RANGE,
    // ── Results ──
    POSITIVE, NEGATIVE, BORDERLINE, DEFICIENT, NORMAL, ELEVATED,
    CONCENTRATION, REFERENCE_RANGE, CONFIDENCE,
    // ── Calibration ──
    CAL_START, CAL_ADD_POINT, CAL_FINISH, CAL_VALIDATE, CAL_SAVED,
    CAL_POOR_FIT, CAL_EXPIRED,
    // ── Patient ──
    ADD_PATIENT, PATIENT_ID, SELECT_PATIENT, NO_PATIENTS,
    // ── Settings ──
    LANGUAGE, WIFI_SETTINGS, ABOUT, DIAGNOSTICS, FACTORY_RESET,
    FIRMWARE_VERSION, DEVICE_ID,
    // ── System ──
    BATTERY_LOW, MEMORY_LOW, CAMERA_FAIL, CONNECTING,
    CONNECTED, DISCONNECTED, SYNCING, SYNC_COMPLETE,
    // ── General ──
    OK_BTN, BACK, SAVE, DELETE_BTN, CONFIRM, YES, NO_BTN, ERROR_TITLE,
    _COUNT
};

static constexpr size_t NUM_LANGS   = static_cast<size_t>(Language::_COUNT);
static constexpr size_t NUM_STRINGS = static_cast<size_t>(StringID::_COUNT);

// ─── String Table (progmem-style, stored in .rodata) ──────────────────
// Indexed as: table[string_id][language]
static const char* const STRING_TABLE[NUM_STRINGS][NUM_LANGS] = {
    // HOME
    {"Startseite", "Home", "Accueil", "Inicio", "Home", "Início", "Start", "Główna", "Ana Sayfa"},
    // TEST
    {"Test", "Test", "Test", "Test", "Test", "Teste", "Test", "Test", "Test"},
    // RESULTS
    {"Ergebnisse", "Results", "Résultats", "Resultados", "Risultati", "Resultados", "Resultaten", "Wyniki", "Sonuçlar"},
    // CALIBRATE
    {"Kalibrieren", "Calibrate", "Calibrer", "Calibrar", "Calibrare", "Calibrar", "Kalibreren", "Kalibracja", "Kalibrasyon"},
    // PATIENTS
    {"Patienten", "Patients", "Patients", "Pacientes", "Pazienti", "Pacientes", "Patiënten", "Pacjenci", "Hastalar"},
    // SETTINGS
    {"Einstellungen", "Settings", "Paramètres", "Ajustes", "Impostazioni", "Definições", "Instellingen", "Ustawienia", "Ayarlar"},
    // AI_MODE
    {"AI Modus", "AI Mode", "Mode IA", "Modo IA", "Modalità IA", "Modo IA", "AI Modus", "Tryb AI", "AI Modu"},
    // REPORTS
    {"Berichte", "Reports", "Rapports", "Informes", "Report", "Relatórios", "Rapporten", "Raporty", "Raporlar"},
    // QC
    {"Qualität", "Quality", "Qualité", "Calidad", "Qualità", "Qualidade", "Kwaliteit", "Jakość", "Kalite"},
    // EXPORT_BTN
    {"Exportieren", "Export", "Exporter", "Exportar", "Esporta", "Exportar", "Exporteren", "Eksportuj", "Dışa Aktar"},
    // MEASURING
    {"Messung läuft…", "Measuring…", "Mesure en cours…", "Midiendo…", "Misurazione…", "Medindo…", "Meting…", "Pomiar…", "Ölçülüyor…"},
    // CANCEL
    {"Abbrechen", "Cancel", "Annuler", "Cancelar", "Annulla", "Cancelar", "Annuleren", "Anuluj", "İptal"},
    // MEASUREMENT_COMPLETE
    {"Messung abgeschlossen", "Measurement complete", "Mesure terminée", "Medición completa", "Misurazione completata", "Medição concluída", "Meting voltooid", "Pomiar zakończony", "Ölçüm tamamlandı"},
    // NO_PEAKS
    {"Keine Peaks", "No peaks", "Pas de pics", "Sin picos", "Nessun picco", "Sem picos", "Geen pieken", "Brak pików", "Pik yok"},
    // CONTROL_WEAK
    {"Kontrolle schwach", "Control weak", "Contrôle faible", "Control débil", "Controllo debole", "Controle fraco", "Controle zwak", "Kontrola słaba", "Kontrol zayıf"},
    // HIGH_BG
    {"Hohes Rauschen", "High background", "Bruit élevé", "Ruido alto", "Rumore alto", "Ruído alto", "Hoog ruis", "Wysoki szum", "Yüksek gürültü"},
    // LOW_SNR
    {"Niedriges SNR", "Low SNR", "SNR faible", "SNR bajo", "SNR basso", "SNR baixo", "Laag SNR", "Niski SNR", "Düşük SNR"},
    // OUT_OF_RANGE
    {"Außerhalb", "Out of range", "Hors limites", "Fuera de rango", "Fuori scala", "Fora do alcance", "Buiten bereik", "Poza zakresem", "Aralık dışı"},
    // POSITIVE
    {"POSITIV", "POSITIVE", "POSITIF", "POSITIVO", "POSITIVO", "POSITIVO", "POSITIEF", "POZYTYWNY", "POZİTİF"},
    // NEGATIVE
    {"NEGATIV", "NEGATIVE", "NÉGATIF", "NEGATIVO", "NEGATIVO", "NEGATIVO", "NEGATIEF", "NEGATYWNY", "NEGATİF"},
    // BORDERLINE
    {"GRENZWERTIG", "BORDERLINE", "LIMITE", "LÍMITE", "LIMITE", "LIMITE", "GRENSWAARDE", "GRANICZNY", "SINIR"},
    // DEFICIENT
    {"MANGEL", "DEFICIENT", "CARENCE", "DEFICIENTE", "CARENTE", "DEFICIENTE", "TEKORT", "NIEDOBÓR", "EKSİK"},
    // NORMAL
    {"NORMAL", "NORMAL", "NORMAL", "NORMAL", "NORMALE", "NORMAL", "NORMAAL", "NORMALNY", "NORMAL"},
    // ELEVATED
    {"ERHÖHT", "ELEVATED", "ÉLEVÉ", "ELEVADO", "ELEVATO", "ELEVADO", "VERHOOGD", "PODWYŻSZONY", "YÜKSEK"},
    // CONCENTRATION
    {"Konzentration", "Concentration", "Concentration", "Concentración", "Concentrazione", "Concentração", "Concentratie", "Stężenie", "Konsantrasyon"},
    // REFERENCE_RANGE
    {"Referenzbereich", "Reference range", "Plage de référence", "Rango de referencia", "Intervallo di riferimento", "Intervalo de referência", "Referentiebereik", "Zakres referencyjny", "Referans aralığı"},
    // CONFIDENCE
    {"Konfidenz", "Confidence", "Confiance", "Confianza", "Confidenza", "Confiança", "Betrouwbaarheid", "Pewność", "Güven"},
    // CAL_START
    {"Kalibrierung starten", "Start calibration", "Démarrer calibration", "Iniciar calibración", "Avvia calibrazione", "Iniciar calibração", "Kalibratie starten", "Rozpocznij kalibrację", "Kalibrasyonu başlat"},
    // CAL_ADD_POINT
    {"Punkt hinzufügen", "Add point", "Ajouter point", "Añadir punto", "Aggiungi punto", "Adicionar ponto", "Punt toevoegen", "Dodaj punkt", "Nokta ekle"},
    // CAL_FINISH
    {"Abschließen", "Finish", "Terminer", "Finalizar", "Concludi", "Finalizar", "Voltooien", "Zakończ", "Bitir"},
    // CAL_VALIDATE
    {"Validieren", "Validate", "Valider", "Validar", "Validare", "Validar", "Valideren", "Waliduj", "Doğrula"},
    // CAL_SAVED
    {"Gespeichert", "Saved", "Enregistré", "Guardado", "Salvato", "Salvo", "Opgeslagen", "Zapisano", "Kaydedildi"},
    // CAL_POOR_FIT
    {"Schlechte Anpassung", "Poor fit", "Mauvais ajustement", "Mal ajuste", "Scarso adattamento", "Ajuste ruim", "Slechte fit", "Słabe dopasowanie", "Kötü uyum"},
    // CAL_EXPIRED
    {"Abgelaufen", "Expired", "Expiré", "Caducado", "Scaduto", "Expirado", "Verlopen", "Wygasła", "Süresi doldu"},
    // ADD_PATIENT
    {"Patient hinzufügen", "Add patient", "Ajouter patient", "Añadir paciente", "Aggiungi paziente", "Adicionar paciente", "Patiënt toevoegen", "Dodaj pacjenta", "Hasta ekle"},
    // PATIENT_ID
    {"Patienten-ID", "Patient ID", "ID Patient", "ID Paciente", "ID Paziente", "ID Paciente", "Patiënt-ID", "ID Pacjenta", "Hasta ID"},
    // SELECT_PATIENT
    {"Patient wählen", "Select patient", "Choisir patient", "Seleccionar paciente", "Seleziona paziente", "Selecionar paciente", "Patiënt kiezen", "Wybierz pacjenta", "Hasta seç"},
    // NO_PATIENTS
    {"Keine Patienten", "No patients", "Aucun patient", "Sin pacientes", "Nessun paziente", "Sem pacientes", "Geen patiënten", "Brak pacjentów", "Hasta yok"},
    // LANGUAGE
    {"Sprache", "Language", "Langue", "Idioma", "Lingua", "Idioma", "Taal", "Język", "Dil"},
    // WIFI_SETTINGS
    {"WLAN", "WiFi", "WiFi", "WiFi", "WiFi", "WiFi", "WiFi", "WiFi", "WiFi"},
    // ABOUT
    {"Über", "About", "À propos", "Acerca de", "Info", "Sobre", "Over", "O programie", "Hakkında"},
    // DIAGNOSTICS
    {"Diagnose", "Diagnostics", "Diagnostics", "Diagnósticos", "Diagnostica", "Diagnósticos", "Diagnostiek", "Diagnostyka", "Tanılama"},
    // FACTORY_RESET
    {"Werksreset", "Factory reset", "Réinitialisation", "Restablecer", "Ripristino", "Redefinir", "Fabrieksreset", "Reset fabryczny", "Fabrika sıfırlama"},
    // FIRMWARE_VERSION
    {"Firmware", "Firmware", "Firmware", "Firmware", "Firmware", "Firmware", "Firmware", "Firmware", "Firmware"},
    // DEVICE_ID
    {"Geräte-ID", "Device ID", "ID Appareil", "ID Dispositivo", "ID Dispositivo", "ID Dispositivo", "Apparaat-ID", "ID Urządzenia", "Cihaz ID"},
    // BATTERY_LOW
    {"Akku niedrig", "Battery low", "Batterie faible", "Batería baja", "Batteria scarica", "Bateria fraca", "Accu laag", "Niski akumulator", "Pil düşük"},
    // MEMORY_LOW
    {"Speicher niedrig", "Memory low", "Mémoire faible", "Memoria baja", "Memoria scarsa", "Memória baixa", "Geheugen laag", "Mało pamięci", "Bellek düşük"},
    // CAMERA_FAIL
    {"Kamerafehler", "Camera error", "Erreur caméra", "Error cámara", "Errore fotocamera", "Erro câmera", "Camerafout", "Błąd kamery", "Kamera hatası"},
    // CONNECTING
    {"Verbinde…", "Connecting…", "Connexion…", "Conectando…", "Connessione…", "Conectando…", "Verbinden…", "Łączenie…", "Bağlanıyor…"},
    // CONNECTED
    {"Verbunden", "Connected", "Connecté", "Conectado", "Connesso", "Conectado", "Verbonden", "Połączono", "Bağlandı"},
    // DISCONNECTED
    {"Getrennt", "Disconnected", "Déconnecté", "Desconectado", "Disconnesso", "Desconectado", "Verbroken", "Rozłączono", "Bağlantı kesildi"},
    // SYNCING
    {"Synchronisiere…", "Syncing…", "Synchronisation…", "Sincronizando…", "Sincronizzazione…", "Sincronizando…", "Synchroniseren…", "Synchronizacja…", "Senkronizasyon…"},
    // SYNC_COMPLETE
    {"Sync fertig", "Sync complete", "Sync terminé", "Sincronizado", "Sincronizzato", "Sincronizado", "Sync voltooid", "Zsynchronizowano", "Senkronize"},
    // OK_BTN
    {"OK", "OK", "OK", "OK", "OK", "OK", "OK", "OK", "OK"},
    // BACK
    {"Zurück", "Back", "Retour", "Atrás", "Indietro", "Voltar", "Terug", "Wstecz", "Geri"},
    // SAVE
    {"Speichern", "Save", "Enregistrer", "Guardar", "Salva", "Salvar", "Opslaan", "Zapisz", "Kaydet"},
    // DELETE_BTN
    {"Löschen", "Delete", "Supprimer", "Eliminar", "Elimina", "Excluir", "Verwijderen", "Usuń", "Sil"},
    // CONFIRM
    {"Bestätigen", "Confirm", "Confirmer", "Confirmar", "Conferma", "Confirmar", "Bevestigen", "Potwierdź", "Onayla"},
    // YES
    {"Ja", "Yes", "Oui", "Sí", "Sì", "Sim", "Ja", "Tak", "Evet"},
    // NO_BTN
    {"Nein", "No", "Non", "No", "No", "Não", "Nee", "Nie", "Hayır"},
    // ERROR_TITLE
    {"Fehler", "Error", "Erreur", "Error", "Errore", "Erro", "Fout", "Błąd", "Hata"},
};

static const char* LANG_NAMES[] = {
    "Deutsch", "English", "Français", "Español",
    "Italiano", "Português", "Nederlands", "Polski", "Türkçe"
};

static const char* LANG_CODES[] = {
    "de", "en", "fr", "es", "it", "pt", "nl", "pl", "tr"
};

class LocalizationManager {
public:
    Result<void> initialize() {
        // Load persisted language preference
        nvs_handle_t h;
        if (nvs_open("settings", NVS_READONLY, &h) == ESP_OK) {
            uint8_t lang = 0;
            if (nvs_get_u8(h, "language", &lang) == ESP_OK) {
                if (lang < NUM_LANGS) lang_ = static_cast<Language>(lang);
            }
            nvs_close(h);
        }
        ESP_LOGI(TAG, "Language: %s (%s)", LANG_NAMES[static_cast<uint8_t>(lang_)],
                 LANG_CODES[static_cast<uint8_t>(lang_)]);
        return Ok();
    }

    const char* get(StringID id) const {
        size_t sid = static_cast<size_t>(id);
        size_t lid = static_cast<size_t>(lang_);
        if (sid >= NUM_STRINGS) return "???";
        const char* s = STRING_TABLE[sid][lid];
        return s ? s : STRING_TABLE[sid][1]; // Fallback to English
    }

    // Convenience operator
    const char* operator()(StringID id) const { return get(id); }

    Result<void> setLanguage(Language lang) {
        lang_ = lang;
        nvs_handle_t h;
        if (nvs_open("settings", NVS_READWRITE, &h) == ESP_OK) {
            nvs_set_u8(h, "language", static_cast<uint8_t>(lang));
            nvs_commit(h);
            nvs_close(h);
        }
        ESP_LOGI(TAG, "Language set: %s", LANG_NAMES[static_cast<uint8_t>(lang_)]);
        return Ok();
    }

    Language     getLanguage()     const { return lang_; }
    const char*  getLanguageName() const { return LANG_NAMES[static_cast<uint8_t>(lang_)]; }
    const char*  getLanguageCode() const { return LANG_CODES[static_cast<uint8_t>(lang_)]; }
    static size_t getLanguageCount() { return NUM_LANGS; }
    static const char* getLanguageName(size_t idx) {
        return idx < NUM_LANGS ? LANG_NAMES[idx] : "?";
    }

private:
    Language lang_ = Language::DE; // Default: German
};

static LocalizationManager s_i18n;
LocalizationManager* getLocalizationManager() { return &s_i18n; }

} // namespace phoenix
