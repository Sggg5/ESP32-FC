#include "rg_localization.h"

static const char *language_names[RG_LANG_MAX] = {
    [RG_LANG_EN] = "English",
    [RG_LANG_FR] = "Francais",
    [RG_LANG_DE] = "Deutsch",
    [RG_LANG_ZH] = "\xE4\xB8\xAD\xE6\x96\x87",
};

static const char *translations[][RG_LANG_MAX] =
{
    {
        [RG_LANG_EN] = "Never",
        [RG_LANG_FR] = "Jamais",
        [RG_LANG_DE] = "Niemals",
        [RG_LANG_ZH] = "\xE4\xBB\x8E\xE4\xB8\x8D",
    },
    {
        [RG_LANG_EN] = "Always",
        [RG_LANG_FR] = "Toujours",
        [RG_LANG_DE] = "Immer",
        [RG_LANG_ZH] = "\xE6\x80\xBB\xE6\x98\xAF",
    },
    {
        [RG_LANG_EN] = "Composite",
        [RG_LANG_FR] = "Composite",
        [RG_LANG_DE] = "Composite",
        [RG_LANG_ZH] = "\xE5\xA4\x8D\xE5\x90\x88",
    },
    {
        [RG_LANG_EN] = "NES Classic",
        [RG_LANG_FR] = "NES Classic",
        [RG_LANG_DE] = "NES Classic",
        [RG_LANG_ZH] = "\x4E\x45\x53\x20\xE7\xBB\x8F\xE5\x85\xB8",
    },
    {
        [RG_LANG_EN] = "NTSC",
        [RG_LANG_FR] = "NTSC",
        [RG_LANG_DE] = "NTSC",
        [RG_LANG_ZH] = "\x4E\x54\x53\x43",
    },
    {
        [RG_LANG_EN] = "PVM",
        [RG_LANG_FR] = "PVM",
        [RG_LANG_DE] = "PVM",
        [RG_LANG_ZH] = "\x50\x56\x4D",
    },
    {
        [RG_LANG_EN] = "Smooth",
        [RG_LANG_FR] = "Lisser",
        [RG_LANG_DE] = "Glatt",
        [RG_LANG_ZH] = "\xE5\xB9\xB3\xE6\xBB\x91",
    },
    {
        [RG_LANG_EN] = "To start, try: 1 or * or #",
        [RG_LANG_FR] = "Pour commencer, 1 ou * ou #",
        [RG_LANG_DE] = "Zum Starten, versuche 1 oder * oder #",
        [RG_LANG_ZH] = "\xE5\xBC\x80\xE5\xA7\x8B\xE8\xAF\xB7\xE6\x8C\x89\x3A\x20\x31\x20\xE6\x88\x96\x20\x2A\x20\xE6\x88\x96\x20\x23",
    },
    {
	    [RG_LANG_EN] = "Full",
	    [RG_LANG_FR] = "Complet",
        [RG_LANG_DE] = "Voll",
        [RG_LANG_ZH] = "\xE5\x85\xA8\xE5\xB1\x8F",
    },
    {
        [RG_LANG_EN] = "Yes",
        [RG_LANG_FR] = "Oui",
        [RG_LANG_DE] = "Ja",
        [RG_LANG_ZH] = "\xE6\x98\xAF",
    },
    {
        [RG_LANG_EN] = "Select file",
        [RG_LANG_FR] = "Choisissez un fichier",
        [RG_LANG_DE] = "Datei auswählen",
        [RG_LANG_ZH] = "\xE9\x80\x89\xE6\x8B\xA9\xE6\x96\x87\xE4\xBB\xB6",
    },
    {
        [RG_LANG_EN] = "Language",
        [RG_LANG_FR] = "Langue",
        [RG_LANG_DE] = "Sprache",
        [RG_LANG_ZH] = "\xE8\xAF\xAD\xE8\xA8\x80",
    },
    {
        [RG_LANG_EN] = "Language changed!",
        [RG_LANG_FR] = "Changement de langue",
        [RG_LANG_DE] = "Sprache geändert!",
        [RG_LANG_ZH] = "\xE8\xAF\xAD\xE8\xA8\x80\xE5\xB7\xB2\xE6\x9B\xB4\xE6\x94\xB9\x21",
    },
    {
        [RG_LANG_EN] = "For these changes to take effect you must restart your device.\nrestart now?",
        [RG_LANG_FR] = "Pour que ces changements prennent effet, vous devez redémarrer votre appareil.\nRedémarrer maintenant ?",
        [RG_LANG_DE] = "Um die Änderungen anzuwenden muss das Gerät neu gestartet werden. \nJetzt neustarten?",
    },
    {
        [RG_LANG_EN] = "Wi-Fi profile",
        [RG_LANG_FR] = "Profil Wi-Fi",
        [RG_LANG_DE] = "Wi-Fi Profil",
    },
    {
        [RG_LANG_EN] = "Language",
        [RG_LANG_FR] = "Langue",
        [RG_LANG_DE] = "Sprache",
        [RG_LANG_ZH] = "\xE8\xAF\xAD\xE8\xA8\x80",
    },
    {
        [RG_LANG_EN] = "Options",
        [RG_LANG_FR] = "Options",
        [RG_LANG_DE] = "Optionen",
        [RG_LANG_ZH] = "\xE9\x80\x89\xE9\xA1\xB9",
    },
    {
        [RG_LANG_EN] = "About Retro-Go",
        [RG_LANG_FR] = "A propos de Retro-go",
        [RG_LANG_DE] = "Über Retro-Go",
        [RG_LANG_ZH] = "\xE5\x85\xB3\xE4\xBA\x8E\x20\x52\x65\x74\x72\x6F\x2D\x47\x6F",
    },
    {
        [RG_LANG_EN] = "Reset all settings?",
        [RG_LANG_FR] = "Reset tous les paramètres",
        [RG_LANG_DE] = "Alle Einstellungen zurücksetzen?",
        [RG_LANG_ZH] = "\xE9\x87\x8D\xE7\xBD\xAE\xE6\x89\x80\xE6\x9C\x89\xE8\xAE\xBE\xE7\xBD\xAE\x3F",
    },
    {
        [RG_LANG_EN] = "Initializing...",
        [RG_LANG_FR] = "Initialisation...",
        [RG_LANG_DE] = "Initialisierung...",
        [RG_LANG_ZH] = "\xE5\x88\x9D\xE5\xA7\x8B\xE5\x8C\x96\x2E\x2E\x2E",
    },
    {
        [RG_LANG_EN] = "Host Game (P1)",
        [RG_LANG_FR] = "Host Game (P1)",
        [RG_LANG_DE] = "Hoste Spiel (P1)",
        [RG_LANG_ZH] = "\xE4\xB8\xBB\xE6\x9C\xBA\xE6\xB8\xB8\xE6\x88\x8F\x28\x50\x31\x29",
    },
    {
        [RG_LANG_EN] = "Find Game (P2)",
        [RG_LANG_FR] = "Find Game (P2)",
        [RG_LANG_DE] = "Finde Spiel (P2)",
        [RG_LANG_ZH] = "\xE6\x9F\xA5\xE6\x89\xBE\xE6\xB8\xB8\xE6\x88\x8F\x28\x50\x32\x29",
    },
    {
        [RG_LANG_EN] = "Netplay",
        [RG_LANG_FR] = "Netplay",
        [RG_LANG_DE] = "Netplay",
        [RG_LANG_ZH] = "\xE8\x81\x94\xE7\xBD\x91\xE6\xB8\xB8\xE6\x88\x8F",
    },
    {
        [RG_LANG_EN] = "ROMs not identical. Continue?",
        [RG_LANG_FR] = "ROMs not identical. Continue?",
        [RG_LANG_DE] = "ROMs nicht identisch. Weiter?",
        [RG_LANG_ZH] = "\x52\x4F\x4D\x20\xE4\xB8\x8D\xE4\xB8\x80\xE8\x87\xB4\x2C\x20\xE7\xBB\xA7\xE7\xBB\xAD\x3F",
    },
    {
        [RG_LANG_EN] = "Exchanging info...",
        [RG_LANG_FR] = "Exchanging info...",
        [RG_LANG_DE] = "Informationsaustausch...",
        [RG_LANG_ZH] = "\xE4\xBA\xA4\xE6\x8D\xA2\xE4\xBF\xA1\xE6\x81\xAF\x2E\x2E\x2E",
    },
    {
        [RG_LANG_EN] = "Unable to find host!",
        [RG_LANG_FR] = "Unable to find host!",
        [RG_LANG_DE] = "Kein Host gefunden!",
        [RG_LANG_ZH] = "\xE6\x89\xBE\xE4\xB8\x8D\xE5\x88\xB0\xE4\xB8\xBB\xE6\x9C\xBA\x21",
    },
    {
        [RG_LANG_EN] = "Connection failed!",
        [RG_LANG_FR] = "Connection failed!",
        [RG_LANG_DE] = "Verbindung fehlgeschlagen!",
        [RG_LANG_ZH] = "\xE8\xBF\x9E\xE6\x8E\xA5\xE5\xA4\xB1\xE8\xB4\xA5\x21",
    },
    {
        [RG_LANG_EN] = "Waiting for peer...",
        [RG_LANG_FR] = "Waiting for peer...",
        [RG_LANG_DE] = "Warte auf peer",
        [RG_LANG_ZH] = "\xE7\xAD\x89\xE5\xBE\x85\xE5\xAF\xB9\xE7\xAB\xAF\x2E\x2E\x2E",
    },
    {
        [RG_LANG_EN] = "Unknown status...",
        [RG_LANG_FR] = "Unknown status...",
        [RG_LANG_DE] = "Unbekannter Status...",
        [RG_LANG_ZH] = "\xE6\x9C\xAA\xE7\x9F\xA5\xE7\x8A\xB6\xE6\x80\x81\x2E\x2E\x2E",
    },
    {
        [RG_LANG_EN] = "On",
        [RG_LANG_FR] = "On",
        [RG_LANG_DE] = "An",
        [RG_LANG_ZH] = "\xE5\xBC\x80",
    },
    {
        [RG_LANG_EN] = "Keyboard",
        [RG_LANG_FR] = "Clavier",
        [RG_LANG_DE] = "Keyboard",
        [RG_LANG_ZH] = "\xE9\x94\xAE\xE7\x9B\x98",
    },
    {
        [RG_LANG_EN] = "Joystick",
        [RG_LANG_FR] = "Joystick",
        [RG_LANG_DE] = "Joystick",
        [RG_LANG_ZH] = "\xE6\x91\x87\xE6\x9D\x86",
    },
    {
        [RG_LANG_EN] = "Input",
        [RG_LANG_FR] = "Entrée",
        [RG_LANG_DE] = "Eingabe",
        [RG_LANG_ZH] = "\xE8\xBE\x93\xE5\x85\xA5",
    },
    {
        [RG_LANG_EN] = "Crop",
        [RG_LANG_FR] = "Couper",
        [RG_LANG_DE] = "Zuschneiden",
        [RG_LANG_ZH] = "\xE8\xA3\x81\xE5\x89\xAA",
    },
    {
        [RG_LANG_EN] = "BIOS file missing!",
        [RG_LANG_FR] = "Fichier BIOS manquant",
        [RG_LANG_DE] = "BIOS-Datei fehlt!",
        [RG_LANG_ZH] = "\xE7\xBC\xBA\xE5\xB0\x91\x20\x42\x49\x4F\x53\x20\xE6\x96\x87\xE4\xBB\xB6\x21",
    },
    {
        [RG_LANG_EN] = "YM2612 audio ",
        [RG_LANG_FR] = "YM2612 audio ",
        [RG_LANG_DE] = "YM2612 audio ",
    },
    {
        [RG_LANG_EN] = "SN76489 audio",
        [RG_LANG_FR] = "SN76489 audio",
        [RG_LANG_DE] = "SN76489 audio",
    },
    {
        [RG_LANG_EN] = "Z80 emulation",
        [RG_LANG_FR] = "Emulation Z80",
        [RG_LANG_DE] = "Z80 emulation",
    },
    {
        [RG_LANG_EN] = "Launcher options",
        [RG_LANG_FR] = "Options du lanceur",
        [RG_LANG_DE] = "Launcher Optionen",
        [RG_LANG_ZH] = "\xE5\x90\xAF\xE5\x8A\xA8\xE5\x99\xA8\xE9\x80\x89\xE9\xA1\xB9",
    },
    {
        [RG_LANG_EN] = "Emulator options",
        [RG_LANG_FR] = "Options émulateur",
        [RG_LANG_DE] = "Emulator Optionen",
        [RG_LANG_ZH] = "\xE6\xA8\xA1\xE6\x8B\x9F\xE5\x99\xA8\xE9\x80\x89\xE9\xA1\xB9",
    },
    {
        [RG_LANG_EN] = "Date",
        [RG_LANG_FR] = "Date",
        [RG_LANG_DE] = "Datum",
        [RG_LANG_ZH] = "\xE6\x97\xA5\xE6\x9C\x9F",
    },
    {
        [RG_LANG_EN] = "Files:",
        [RG_LANG_FR] = "Fichiers:",
        [RG_LANG_DE] = "Dateien",
        [RG_LANG_ZH] = "\xE6\x96\x87\xE4\xBB\xB6\x3A",
    },
    {
        [RG_LANG_EN] = "Download complete!",
        [RG_LANG_FR] = "Téléchargement terminé",
        [RG_LANG_DE] = "Download abgeschlossen!",
        [RG_LANG_ZH] = "\xE4\xB8\x8B\xE8\xBD\xBD\xE5\xAE\x8C\xE6\x88\x90\x21",
    },
    {
        [RG_LANG_EN] = "Reboot to flash?",
        [RG_LANG_FR] = "Redémarrer",
        [RG_LANG_DE] = "Neustarten um zu flashen",
        [RG_LANG_ZH] = "\xE9\x87\x8D\xE5\x90\xAF\xE5\x88\xB7\xE5\x86\x99\x3F",
    },
    {
        [RG_LANG_EN] = "Available Releases",
        [RG_LANG_FR] = "Maj disponible",
        [RG_LANG_DE] = "Verfügbare Releases",
        [RG_LANG_ZH] = "\xE5\x8F\xAF\xE7\x94\xA8\xE7\x89\x88\xE6\x9C\xAC",
    },
    {
        [RG_LANG_EN] = "Received empty list!",
        [RG_LANG_FR] = "Liste vide reçue",
        [RG_LANG_DE] = "Leere Liste empfangen!",
        [RG_LANG_ZH] = "\xE6\x94\xB6\xE5\x88\xB0\xE7\xA9\xBA\xE5\x88\x97\xE8\xA1\xA8\x21",
    },
    {
        [RG_LANG_EN] = "Gamma Boost",
        [RG_LANG_FR] = "Boost Gamma",
        [RG_LANG_DE] = "Gamma Boost",
        [RG_LANG_ZH] = "\xE4\xBC\xBD\xE9\xA9\xAC\xE5\xA2\x9E\xE5\xBC\xBA",
    },
    {
        [RG_LANG_EN] = "Day",
        [RG_LANG_FR] = "Jour",
        [RG_LANG_DE] = "Tag",
        [RG_LANG_ZH] = "\xE6\x97\xA5",
    },
    {
        [RG_LANG_EN] = "Hour",
        [RG_LANG_FR] = "Heure",
        [RG_LANG_DE] = "Stunde",
        [RG_LANG_ZH] = "\xE6\x97\xB6",
    },
    {
        [RG_LANG_EN] = "Min",
        [RG_LANG_FR] = "Min",
        [RG_LANG_DE] = "Min",
        [RG_LANG_ZH] = "\xE5\x88\x86",
    },
    {
        [RG_LANG_EN] = "Sec",
        [RG_LANG_FR] = "Sec",
        [RG_LANG_DE] = "Sek",
        [RG_LANG_ZH] = "\xE7\xA7\x92",
    },
    {
        [RG_LANG_EN] = "Sync",
        [RG_LANG_FR] = "Synchro",
        [RG_LANG_DE] = "Synchronisieren",
        [RG_LANG_ZH] = "\xE5\x90\x8C\xE6\xAD\xA5",
    },
    {
        [RG_LANG_EN] = "RTC config",
        [RG_LANG_FR] = "Config RTC",
        [RG_LANG_DE] = "RTC Einstellung",
        [RG_LANG_ZH] = "\xE6\x97\xB6\xE9\x92\x9F\xE8\xAE\xBE\xE7\xBD\xAE",
    },
    {
        [RG_LANG_EN] = "Palette",
        [RG_LANG_FR] = "Palette",
        [RG_LANG_DE] = "Palette",
        [RG_LANG_ZH] = "\xE8\xB0\x83\xE8\x89\xB2\xE6\x9D\xBF",
    },
    {
        [RG_LANG_EN] = "RTC config",
        [RG_LANG_FR] = "Config RTC",
        [RG_LANG_DE] = "RTC Einstellung",
        [RG_LANG_ZH] = "\xE6\x97\xB6\xE9\x92\x9F\xE8\xAE\xBE\xE7\xBD\xAE",
    },
    {
        [RG_LANG_EN] = "SRAM autosave",
        [RG_LANG_FR] = "Sauvegarde auto SRAM",
        [RG_LANG_DE] = "SRAM autospeichern",
        [RG_LANG_ZH] = "\x53\x52\x41\x4D\x20\xE8\x87\xAA\xE5\x8A\xA8\xE4\xBF\x9D\xE5\xAD\x98",
    },
    {
        [RG_LANG_EN] = "Enable BIOS",
        [RG_LANG_FR] = "Activer BIOS",
        [RG_LANG_DE] = "BIOS aktivieren",
        [RG_LANG_ZH] = "\xE5\x90\xAF\xE7\x94\xA8\x20\x42\x49\x4F\x53",
    },
    {
        [RG_LANG_EN] = "Name",
        [RG_LANG_FR] = "Nom",
        [RG_LANG_DE] = "Name",
        [RG_LANG_ZH] = "\xE5\x90\x8D\xE7\xA7\xB0",
    },
    {
        [RG_LANG_EN] = "Artist",
        [RG_LANG_FR] = "Artiste",
        [RG_LANG_DE] = "Künstler",
        [RG_LANG_ZH] = "\xE4\xBD\x9C\xE8\x80\x85",
    },
    {
        [RG_LANG_EN] = "Copyright",
        [RG_LANG_FR] = "Copyright",
        [RG_LANG_DE] = "Copyright",
        [RG_LANG_ZH] = "\xE7\x89\x88\xE6\x9D\x83",
    },
    {
        [RG_LANG_EN] = "Playing",
        [RG_LANG_FR] = "Playing",
        [RG_LANG_DE] = "Spielt",
        [RG_LANG_ZH] = "\xE6\xAD\xA3\xE5\x9C\xA8\xE6\x92\xAD\xE6\x94\xBE",
    },
    {
        [RG_LANG_EN] = "Palette",
        [RG_LANG_FR] = "Palette",
        [RG_LANG_DE] = "Palette",
        [RG_LANG_ZH] = "\xE8\xB0\x83\xE8\x89\xB2\xE6\x9D\xBF",
    },
    {
        [RG_LANG_EN] = "Overscan",
        [RG_LANG_FR] = "Overscan",
        [RG_LANG_DE] = "Overscan",
        [RG_LANG_ZH] = "\xE8\xBF\x87\xE6\x89\xAB\xE6\x8F\x8F",
    },
    {
        [RG_LANG_EN] = "Crop sides",
        [RG_LANG_FR] = "Couper les côtés",
        [RG_LANG_DE] = "Seiten abschneiden",
        [RG_LANG_ZH] = "\xE8\xA3\x81\xE8\xBE\xB9",
    },
    {
        [RG_LANG_EN] = "Sprite limit",
        [RG_LANG_FR] = "Limite de sprite",
        [RG_LANG_DE] = "Sprite Limit",
        [RG_LANG_ZH] = "\xE7\xB2\xBE\xE7\x81\xB5\xE9\x99\x90\xE5\x88\xB6",
    },
    {
        [RG_LANG_EN] = "Overscan",
        [RG_LANG_FR] = "Overscan",
        [RG_LANG_DE] = "Overscan",
        [RG_LANG_ZH] = "\xE8\xBF\x87\xE6\x89\xAB\xE6\x8F\x8F",
    },
    {
        [RG_LANG_EN] = "Palette",
        [RG_LANG_FR] = "Palette",
        [RG_LANG_DE] = "Palette",
        [RG_LANG_ZH] = "\xE8\xB0\x83\xE8\x89\xB2\xE6\x9D\xBF",
    },
    {
        [RG_LANG_EN] = "Profile",
        [RG_LANG_FR] = "Profil",
        [RG_LANG_DE] = "Profil",
        [RG_LANG_ZH] = "\xE9\x85\x8D\xE7\xBD\xAE",
    },
    {
        [RG_LANG_EN] = "<profile name>",
        [RG_LANG_FR] = "<nom du profil>",
        [RG_LANG_DE] = "<profil name>",
        [RG_LANG_ZH] = "\x3C\xE9\x85\x8D\xE7\xBD\xAE\xE5\x90\x8D\x3E",
    },
    {
        [RG_LANG_EN] = "Controls",
        [RG_LANG_FR] = "Contrôles",
        [RG_LANG_DE] = "Steuerung",
        [RG_LANG_ZH] = "\xE6\x8E\xA7\xE5\x88\xB6",
    },
    {
        [RG_LANG_EN] = "Audio enable",
        [RG_LANG_FR] = "Activer audio",
        [RG_LANG_DE] = "Audio einschalten",
        [RG_LANG_ZH] = "\xE5\x90\xAF\xE7\x94\xA8\xE5\xA3\xB0\xE9\x9F\xB3",
    },
    {
        [RG_LANG_EN] = "Audio filter",
        [RG_LANG_FR] = "Filtre audio",
        [RG_LANG_DE] = "Audio Filter",
        [RG_LANG_ZH] = "\xE5\xA3\xB0\xE9\x9F\xB3\xE6\xBB\xA4\xE6\xB3\xA2",
    },


    // rg_gui.c
    {
        [RG_LANG_EN] = "Folder is empty.",
        [RG_LANG_FR] = "Le dossier est vide.",
        [RG_LANG_DE] = "Ordner ist leer",
        [RG_LANG_ZH] = "\xE6\x96\x87\xE4\xBB\xB6\xE5\xA4\xB9\xE4\xB8\xBA\xE7\xA9\xBA\x2E",
    },
    {
        [RG_LANG_EN] = "No",
        [RG_LANG_FR] = "Non",
        [RG_LANG_DE] = "Nein",
        [RG_LANG_ZH] = "\xE5\x90\xA6",
    },
    {
        [RG_LANG_EN] = "OK",
        [RG_LANG_FR] = "OK",
        [RG_LANG_DE] = "OK",
        [RG_LANG_ZH] = "\xE7\xA1\xAE\xE5\xAE\x9A",
    },
    {
        [RG_LANG_EN] = "On",
        [RG_LANG_FR] = "On",
        [RG_LANG_DE] = "An",
        [RG_LANG_ZH] = "\xE5\xBC\x80",
    },
    {
        [RG_LANG_EN] = "Off",
        [RG_LANG_FR] = "Off",
        [RG_LANG_DE] = "Aus",
        [RG_LANG_ZH] = "\xE5\x85\xB3",
    },
    {
        [RG_LANG_EN] = "Horiz",
        [RG_LANG_FR] = "Horiz",
        [RG_LANG_DE] = "Horiz",
        [RG_LANG_ZH] = "\xE6\xB0\xB4\xE5\xB9\xB3",
    },
    {
        [RG_LANG_EN] = "Vert",
        [RG_LANG_FR] = "Vert ",
        [RG_LANG_DE] = "Vert",
        [RG_LANG_ZH] = "\xE5\x9E\x82\xE7\x9B\xB4",
    },
    {
        [RG_LANG_EN] = "Both",
        [RG_LANG_FR] = "Tout",
        [RG_LANG_DE] = "Beide",
        [RG_LANG_ZH] = "\xE5\x8F\x8C\xE5\x90\x91",
    },
    {
        [RG_LANG_EN] = "Fit",
        [RG_LANG_FR] = "Ajuster",
        [RG_LANG_DE] = "Einpassen",
        [RG_LANG_ZH] = "\xE9\x80\x82\xE5\xBA\x94",
    },
    {
        [RG_LANG_EN] = "Full ",
        [RG_LANG_FR] = "Remplir ",
        [RG_LANG_DE] = "Voll",
        [RG_LANG_ZH] = "\xE5\x85\xA8\xE5\xB1\x8F",
    },
    {
        [RG_LANG_EN] = "Zoom",
        [RG_LANG_FR] = "Zoomer",
        [RG_LANG_DE] = "Zoom",
        [RG_LANG_ZH] = "\xE7\xBC\xA9\xE6\x94\xBE",
    },

    // Led options
    {
        [RG_LANG_EN] = "LED options",
        [RG_LANG_FR] = "Options LED",
        [RG_LANG_DE] = "LED Optionen",
    },
    {
        [RG_LANG_EN] = "System activity",
        [RG_LANG_FR] = "Activité système",
        [RG_LANG_DE] = "Systemaktivität",
    },
    {
        [RG_LANG_EN] = "Disk activity",
        [RG_LANG_FR] = "Activité stockage",
        [RG_LANG_DE] = "SD-Aktivität",
    },
    {
        [RG_LANG_EN] = "Low battery",
        [RG_LANG_FR] = "Batterie basse",
        [RG_LANG_DE] = "Schwache Batterie",
    },
    {
        [RG_LANG_EN] = "Default",
        [RG_LANG_FR] = "Défaut",
        [RG_LANG_DE] = "Standard",
        [RG_LANG_ZH] = "\xE9\xBB\x98\xE8\xAE\xA4",
    },
    {
        [RG_LANG_EN] = "<None>",
        [RG_LANG_FR] = "<Aucun>",
        [RG_LANG_DE] = "<None>",
        [RG_LANG_ZH] = "\x3C\xE6\x97\xA0\x3E",
    },

    // Wifi
    {
        [RG_LANG_EN] = "Not connected",
        [RG_LANG_FR] = "Non connecté",
        [RG_LANG_DE] = "Nicht verbunden",
        [RG_LANG_ZH] = "\xE6\x9C\xAA\xE8\xBF\x9E\xE6\x8E\xA5",
    },
    {
        [RG_LANG_EN] = "Connecting...",
        [RG_LANG_FR] = "Connexion...",
        [RG_LANG_DE] = "Verbinde...",
        [RG_LANG_ZH] = "\xE8\xBF\x9E\xE6\x8E\xA5\xE4\xB8\xAD\x2E\x2E\x2E",
    },
    {
        [RG_LANG_EN] = "Disconnecting...",
        [RG_LANG_FR] = "Déconnexion...",
        [RG_LANG_DE] = "Trennen...",
        [RG_LANG_ZH] = "\xE6\x96\xAD\xE5\xBC\x80\xE4\xB8\xAD\x2E\x2E\x2E",
    },
    {
        [RG_LANG_EN] = "(empty)",
        [RG_LANG_FR] = "(vide)",
        [RG_LANG_DE] = "(leer)",
        [RG_LANG_ZH] = "\x28\xE7\xA9\xBA\x29",
    },
    {
        [RG_LANG_EN] = "Wi-Fi AP",
        [RG_LANG_FR] = "Wi-Fi AP",
        [RG_LANG_DE] = "Wi-Fi AP",
        [RG_LANG_ZH] = "\x57\x69\x2D\x46\x69\x20\xE7\x83\xAD\xE7\x82\xB9",
    },
    {
        [RG_LANG_EN] = "Start access point?\n\nSSID: retro-go\nPassword: retro-go\n\nBrowse: http://192.168.4.1/",
        [RG_LANG_FR] = "Démarrer point d'accès?\n\nSSID: retro-go\nPassword: retro-go\n\nAdresse: http://192.168.4.1/",
        [RG_LANG_DE] = "Starte Hotspot?\n\nSSID: retro-go\nPasswort: retro-go\n\nAdresse: http://192.168.4.1/",
    },
    {
        [RG_LANG_EN] = "Wi-Fi enable",
        [RG_LANG_FR] = "Activer Wi-Fi",
        [RG_LANG_DE] = "Wi-Fi aktivieren",
        [RG_LANG_ZH] = "\xE5\x90\xAF\xE7\x94\xA8\x20\x57\x69\x2D\x46\x69",
    },
    {
        [RG_LANG_EN] = "Wi-Fi access point",
        [RG_LANG_FR] = "Point d'accès WiFi",
        [RG_LANG_DE] = "Wi-Fi Hotspot",
        [RG_LANG_ZH] = "\x57\x69\x2D\x46\x69\x20\xE7\x83\xAD\xE7\x82\xB9",
    },
    {
        [RG_LANG_EN] = "Network",
        [RG_LANG_FR] = "Réseau",
        [RG_LANG_DE] = "Netzwerk",
        [RG_LANG_ZH] = "\xE7\xBD\x91\xE7\xBB\x9C",
    },
    {
        [RG_LANG_EN] = "IP address",
        [RG_LANG_FR] = "Adresse IP",
        [RG_LANG_DE] = "IP-Adresse",
        [RG_LANG_ZH] = "\x49\x50\x20\xE5\x9C\xB0\xE5\x9D\x80",
    },

    // retro-go settings
    {
        [RG_LANG_EN] = "Brightness",
        [RG_LANG_FR] = "Luminosité",
        [RG_LANG_DE] = "Helligkeit",
        [RG_LANG_ZH] = "\xE4\xBA\xAE\xE5\xBA\xA6",
    },
    {
        [RG_LANG_EN] = "Volume",
        [RG_LANG_FR] = "Volume",
        [RG_LANG_DE] = "Lautstärke",
        [RG_LANG_ZH] = "\xE9\x9F\xB3\xE9\x87\x8F",
    },
    {
        [RG_LANG_EN] = "Audio out",
        [RG_LANG_FR] = "Sortie audio",
        [RG_LANG_DE] = "Audioausgang",
        [RG_LANG_ZH] = "\xE5\xA3\xB0\xE9\x9F\xB3\xE8\xBE\x93\xE5\x87\xBA",
    },
    {
        [RG_LANG_EN] = "Font type",
        [RG_LANG_FR] = "Police",
        [RG_LANG_DE] = "Schriftart",
        [RG_LANG_ZH] = "\xE5\xAD\x97\xE4\xBD\x93",
    },
    {
        [RG_LANG_EN] = "Theme",
        [RG_LANG_FR] = "Thème",
        [RG_LANG_DE] = "Thema",
        [RG_LANG_ZH] = "\xE4\xB8\xBB\xE9\xA2\x98",
    },
    {
        [RG_LANG_EN] = "Show clock",
        [RG_LANG_FR] = "Horloge",
        [RG_LANG_DE] = "Uhr anzeigen",
        [RG_LANG_ZH] = "\xE6\x98\xBE\xE7\xA4\xBA\xE6\x97\xB6\xE9\x92\x9F",
    },
    {
        [RG_LANG_EN] = "Timezone",
        [RG_LANG_FR] = "Fuseau",
        [RG_LANG_DE] = "Zeitzone",
        [RG_LANG_ZH] = "\xE6\x97\xB6\xE5\x8C\xBA",
    },
    {
        [RG_LANG_EN] = "Wi-Fi options",
        [RG_LANG_FR] = "Options Wi-Fi",
        [RG_LANG_DE] = "Wi-Fi Optionen",
        [RG_LANG_ZH] = "\x57\x69\x2D\x46\x69\x20\xE9\x80\x89\xE9\xA1\xB9",
    },

    // app settings
    {
        [RG_LANG_EN] = "Scaling",
        [RG_LANG_FR] = "Format",
        [RG_LANG_DE] = "Skalierung",
        [RG_LANG_ZH] = "\xE7\xBC\xA9\xE6\x94\xBE",
    },
    {
        [RG_LANG_EN] = "Factor",
        [RG_LANG_FR] = "Factor",
        [RG_LANG_DE] = "Faktor",
        [RG_LANG_ZH] = "\xE5\x80\x8D\xE7\x8E\x87",
    },
    {
        [RG_LANG_EN] = "Filter",
        [RG_LANG_FR] = "Filtre",
        [RG_LANG_DE] = "Filter",
        [RG_LANG_ZH] = "\xE6\xBB\xA4\xE6\xB3\xA2",
    },
    {
        [RG_LANG_EN] = "Border",
        [RG_LANG_FR] = "Bordure",
        [RG_LANG_DE] = "Rand",
        [RG_LANG_ZH] = "\xE8\xBE\xB9\xE6\xA1\x86",
    },
    {
        [RG_LANG_EN] = "Speed",
        [RG_LANG_FR] = "Vitesse",
        [RG_LANG_DE] = "Geschwindigkeit",
        [RG_LANG_ZH] = "\xE9\x80\x9F\xE5\xBA\xA6",
    },

    // about menu
    {
        [RG_LANG_EN] = "Version",
        [RG_LANG_FR] = "Version",
        [RG_LANG_DE] = "Version",
        [RG_LANG_ZH] = "\xE7\x89\x88\xE6\x9C\xAC",
    },
    {
        [RG_LANG_EN] = "Date",
        [RG_LANG_FR] = "Date",
        [RG_LANG_DE] = "Datum",
        [RG_LANG_ZH] = "\xE6\x97\xA5\xE6\x9C\x9F",
    },
    {
        [RG_LANG_EN] = "Target",
        [RG_LANG_FR] = "Appareil",
        [RG_LANG_DE] = "Ziel",
        [RG_LANG_ZH] = "\xE7\x9B\xAE\xE6\xA0\x87",
    },
    {
        [RG_LANG_EN] = "Website",
        [RG_LANG_FR] = "Site Web",
        [RG_LANG_DE] = "Webseite",
        [RG_LANG_ZH] = "\xE7\xBD\x91\xE7\xAB\x99",
    },
    {
        [RG_LANG_EN] = "Options",
        [RG_LANG_FR] = "Options",
        [RG_LANG_DE] = "Optionen",
        [RG_LANG_ZH] = "\xE9\x80\x89\xE9\xA1\xB9",
    },
    {
        [RG_LANG_EN] = "View credits",
        [RG_LANG_FR] = "Credits",
        [RG_LANG_DE] = "Credits",
        [RG_LANG_ZH] = "\xE6\x9F\xA5\xE7\x9C\x8B\xE9\xB8\xA3\xE8\xB0\xA2",
    },
    {
        [RG_LANG_EN] = "Debug menu",
        [RG_LANG_FR] = "Menu debug",
        [RG_LANG_DE] = "Debug Menü",
        [RG_LANG_ZH] = "\xE8\xB0\x83\xE8\xAF\x95\xE8\x8F\x9C\xE5\x8D\x95",
    },
    {
        [RG_LANG_EN] = "Reset settings",
        [RG_LANG_FR] = "Reset paramètres",
        [RG_LANG_DE] = "Einstellungen zurücksetzen",
        [RG_LANG_ZH] = "\xE9\x87\x8D\xE7\xBD\xAE\xE8\xAE\xBE\xE7\xBD\xAE",
    },

    // save slot
    {
        [RG_LANG_EN] = "Slot 0",
        [RG_LANG_FR] = "Emplacement 0",
        [RG_LANG_DE] = "Speicherplatz 0",
        [RG_LANG_ZH] = "\xE5\xAD\x98\xE6\xA1\xA3\x20\x30",
    },
    {
        [RG_LANG_EN] = "Slot 1",
        [RG_LANG_FR] = "Emplacement 1",
        [RG_LANG_DE] = "Speicherplatz 1",
        [RG_LANG_ZH] = "\xE5\xAD\x98\xE6\xA1\xA3\x20\x31",
    },
    {
        [RG_LANG_EN] = "Slot 2",
        [RG_LANG_FR] = "Emplacement 2",
        [RG_LANG_DE] = "Speicherplatz 2",
        [RG_LANG_ZH] = "\xE5\xAD\x98\xE6\xA1\xA3\x20\x32",

    },
    {
        [RG_LANG_EN] = "Slot 3",
        [RG_LANG_FR] = "Emplacement 3",
        [RG_LANG_DE] = "Speicherplatz 3",
        [RG_LANG_ZH] = "\xE5\xAD\x98\xE6\xA1\xA3\x20\x33",
    },

    // game menu
    {
        [RG_LANG_EN] = "Save & Continue",
        [RG_LANG_FR] = "Sauver et continuer",
        [RG_LANG_DE] = "Speichern und weiter",
        [RG_LANG_ZH] = "\xE4\xBF\x9D\xE5\xAD\x98\xE5\xB9\xB6\xE7\xBB\xA7\xE7\xBB\xAD",
    },
    {
        [RG_LANG_EN] = "Save & Quit",
        [RG_LANG_FR] = "Sauver et quitter",
        [RG_LANG_DE] = "Speichern und Verlassen",
        [RG_LANG_ZH] = "\xE4\xBF\x9D\xE5\xAD\x98\xE5\xB9\xB6\xE9\x80\x80\xE5\x87\xBA",
    },
    {
        [RG_LANG_EN] = "Load game",
        [RG_LANG_FR] = "Charger partie",
        [RG_LANG_DE] = "Spiel laden",
        [RG_LANG_ZH] = "\xE8\xAF\xBB\xE5\x8F\x96\xE6\xB8\xB8\xE6\x88\x8F",
    },
    {
        [RG_LANG_EN] = "Reset",
        [RG_LANG_FR] = "Reset",
        [RG_LANG_DE] = "Reset",
        [RG_LANG_ZH] = "\xE9\x87\x8D\xE7\xBD\xAE",
    },
    {
        [RG_LANG_EN] = "Netplay",
        [RG_LANG_FR] = "Netplay",
        [RG_LANG_DE] = "Netplay",
        [RG_LANG_ZH] = "\xE8\x81\x94\xE7\xBD\x91\xE6\xB8\xB8\xE6\x88\x8F",
    },
    {
        [RG_LANG_EN] = "About",
        [RG_LANG_FR] = "Infos",
        [RG_LANG_DE] = "Infos",
        [RG_LANG_ZH] = "\xE5\x85\xB3\xE4\xBA\x8E",
    },
    {
        [RG_LANG_EN] = "Quit",
        [RG_LANG_FR] = "Quitter",
        [RG_LANG_DE] = "Verlassen",
        [RG_LANG_ZH] = "\xE9\x80\x80\xE5\x87\xBA",
    },
    {
        [RG_LANG_EN] = "Soft reset",
        [RG_LANG_FR] = "Soft reset",
        [RG_LANG_DE] = "Soft reset",
        [RG_LANG_ZH] = "\xE8\xBD\xAF\xE9\x87\x8D\xE5\x90\xAF",
    },
    {
        [RG_LANG_EN] = "Hard reset",
        [RG_LANG_FR] = "Hard reset",
        [RG_LANG_DE] = "Hard reset",
        [RG_LANG_ZH] = "\xE7\xA1\xAC\xE9\x87\x8D\xE5\x90\xAF",
    },

    {
        [RG_LANG_EN] = "Reset Emulation?",
        [RG_LANG_FR] = "Reset Emulation?",
        [RG_LANG_DE] = "Emulation zurücksetzen?",
        [RG_LANG_ZH] = "\xE9\x87\x8D\xE7\xBD\xAE\xE6\xA8\xA1\xE6\x8B\x9F\xE5\x99\xA8\x3F",
    },
    {
        [RG_LANG_EN] = "Save",
        [RG_LANG_FR] = "Sauver",
        [RG_LANG_DE] = "Speichern",
        [RG_LANG_ZH] = "\xE4\xBF\x9D\xE5\xAD\x98",
    },
    {
        [RG_LANG_EN] = "Load",
        [RG_LANG_FR] = "Charger",
        [RG_LANG_DE] = "Laden",
        [RG_LANG_ZH] = "\xE8\xAF\xBB\xE5\x8F\x96",
    },
    // end of rg_gui.c


    // main.c
    {
        [RG_LANG_EN] = "Show",
        [RG_LANG_FR] = "Montrer",
        [RG_LANG_DE] = "Zeigen",
        [RG_LANG_ZH] = "\xE6\x98\xBE\xE7\xA4\xBA",
    },
    {
        [RG_LANG_EN] = "Hide",
        [RG_LANG_FR] = "Cacher",
        [RG_LANG_DE] = "Verstecken",
        [RG_LANG_ZH] = "\xE9\x9A\x90\xE8\x97\x8F",
    },
    {
        [RG_LANG_EN] = "Tabs Visibility",
        [RG_LANG_FR] = "Visibilité onglets",
        [RG_LANG_DE] = "Tabs sichtbar",
        [RG_LANG_ZH] = "\xE6\xA0\x87\xE7\xAD\xBE\xE6\x98\xBE\xE7\xA4\xBA",
    },

    // scroll modes
    {
        [RG_LANG_EN] = "Center",
        [RG_LANG_FR] = "centrer",
        [RG_LANG_DE] = "Zentriert",
        [RG_LANG_ZH] = "\xE5\xB1\x85\xE4\xB8\xAD",
    },
    {
        [RG_LANG_EN] = "Paging",
        [RG_LANG_FR] = "Paging",
        [RG_LANG_DE] = "Blättern",
        [RG_LANG_ZH] = "\xE5\x88\x86\xE9\xA1\xB5",
    },

    // start screen
    {
        [RG_LANG_EN] = "Auto",
        [RG_LANG_FR] = "Auto",
        [RG_LANG_DE] = "Auto",
        [RG_LANG_ZH] = "\xE8\x87\xAA\xE5\x8A\xA8",
    },
    {
        [RG_LANG_EN] = "Carousel",
        [RG_LANG_FR] = "Carousel",
        [RG_LANG_DE] = "Carousel",
        [RG_LANG_ZH] = "\xE8\xBD\xAE\xE6\x92\xAD",
    },
    {
        [RG_LANG_EN] = "Browser",
        [RG_LANG_FR] = "Browser",
        [RG_LANG_DE] = "Browser",
        [RG_LANG_ZH] = "\xE6\xB5\x8F\xE8\xA7\x88\xE5\x99\xA8",
    },

    // preview
    {
        [RG_LANG_EN] = "None",
        [RG_LANG_FR] = "Aucun",
        [RG_LANG_DE] = "Nichts",
        [RG_LANG_ZH] = "\xE6\x97\xA0",
    },
    {
        [RG_LANG_EN] = "Cover,Save",
        [RG_LANG_FR] = "Cover,Save",
        [RG_LANG_DE] = "Cover, Speicherstand",
        [RG_LANG_ZH] = "\xE5\xB0\x81\xE9\x9D\xA2\x2C\xE5\xAD\x98\xE6\xA1\xA3",
    },
    {
        [RG_LANG_EN] = "Save,Cover",
        [RG_LANG_FR] = "Save,Cover",
        [RG_LANG_DE] = "Speicherstand, Cover",
        [RG_LANG_ZH] = "\xE5\xAD\x98\xE6\xA1\xA3\x2C\xE5\xB0\x81\xE9\x9D\xA2",
    },
    {
        [RG_LANG_EN] = "Cover only",
        [RG_LANG_FR] = "Cover only",
        [RG_LANG_DE] = "Nur Cover",
        [RG_LANG_ZH] = "\xE4\xBB\x85\xE5\xB0\x81\xE9\x9D\xA2",
    },
    {
        [RG_LANG_EN] = "Save only",
        [RG_LANG_FR] = "Save only",
        [RG_LANG_DE] = "Nur Save",
        [RG_LANG_ZH] = "\xE4\xBB\x85\xE5\xAD\x98\xE6\xA1\xA3",
    },

    // startup app
    {
        [RG_LANG_EN] = "Last game",
        [RG_LANG_FR] = "Dernier jeu",
        [RG_LANG_DE] = "Letztes Spiel",
        [RG_LANG_ZH] = "\xE4\xB8\x8A\xE6\xAC\xA1\xE6\xB8\xB8\xE6\x88\x8F",
    },
    {
        [RG_LANG_EN] = "Launcher",
        [RG_LANG_FR] = "Launcher",
        [RG_LANG_DE] = "Launcher",
        [RG_LANG_ZH] = "\xE5\x90\xAF\xE5\x8A\xA8\xE5\x99\xA8",
    },

    // launcher options
    {
        [RG_LANG_EN] = "Launcher Options",
        [RG_LANG_FR] = "Options du lanceur",
        [RG_LANG_DE] = "Launcher Optionen",
        [RG_LANG_ZH] = "\xE5\x90\xAF\xE5\x8A\xA8\xE5\x99\xA8\xE9\x80\x89\xE9\xA1\xB9",
    },
    {
        [RG_LANG_EN] = "Color theme",
        [RG_LANG_FR] = "Couleurs",
        [RG_LANG_DE] = "Farbthema",
        [RG_LANG_ZH] = "\xE9\xA2\x9C\xE8\x89\xB2\xE4\xB8\xBB\xE9\xA2\x98",
    },
    {
        [RG_LANG_EN] = "Preview",
        [RG_LANG_FR] = "Aperçu",
        [RG_LANG_DE] = "Vorschau",
        [RG_LANG_ZH] = "\xE9\xA2\x84\xE8\xA7\x88",
    },
    {
        [RG_LANG_EN] = "Scroll mode",
        [RG_LANG_FR] = "Mode défilement",
        [RG_LANG_DE] = "Scroll-Modus",
        [RG_LANG_ZH] = "\xE6\xBB\x9A\xE5\x8A\xA8\xE6\xA8\xA1\xE5\xBC\x8F",
    },
    {
        [RG_LANG_EN] = "Start screen",
        [RG_LANG_FR] = "Écran démarrage",
        [RG_LANG_DE] = "Startbildschirm",
        [RG_LANG_ZH] = "\xE5\x90\xAF\xE5\x8A\xA8\xE9\xA1\xB5",
    },
    {
        [RG_LANG_EN] = "Hide tabs",
        [RG_LANG_FR] = "Cacher onglet",
        [RG_LANG_DE] = "Tabs verstecken",
        [RG_LANG_ZH] = "\xE9\x9A\x90\xE8\x97\x8F\xE6\xA0\x87\xE7\xAD\xBE",
    },
    {
        [RG_LANG_EN] = "File server",
        [RG_LANG_FR] = "Serveur fichier",
        [RG_LANG_DE] = "Datei Server",
        [RG_LANG_ZH] = "\xE6\x96\x87\xE4\xBB\xB6\xE6\x9C\x8D\xE5\x8A\xA1\xE5\x99\xA8",
    },
    {
        [RG_LANG_EN] = "Startup app",
        [RG_LANG_FR] = "App démarrage",
        [RG_LANG_DE] = "Start-App",
        [RG_LANG_ZH] = "\xE5\x90\xAF\xE5\x8A\xA8\xE5\xBA\x94\xE7\x94\xA8",
    },
    {
        [RG_LANG_EN] = "Build CRC cache",
        [RG_LANG_FR] = "Build CRC cache",
        [RG_LANG_DE] = "Build CRC cache",
        [RG_LANG_ZH] = "\xE7\x94\x9F\xE6\x88\x90\x20\x43\x52\x43\x20\xE7\xBC\x93\xE5\xAD\x98",
    },
    {
        [RG_LANG_EN] = "Check for updates",
        [RG_LANG_FR] = "Verifier mise à jour",
        [RG_LANG_DE] = "Auf Updates prüfen",
        [RG_LANG_ZH] = "\xE6\xA3\x80\xE6\x9F\xA5\xE6\x9B\xB4\xE6\x96\xB0",
    },
    {
        [RG_LANG_EN] = "HTTP Server Busy...",
        [RG_LANG_FR] = "Server Web ...",
        [RG_LANG_DE] = "HTTP Server Busy...",
        [RG_LANG_ZH] = "\x48\x54\x54\x50\x20\xE6\x9C\x8D\xE5\x8A\xA1\xE5\xBF\x99\x2E\x2E\x2E",
    },
    {
        [RG_LANG_EN] = "SD Card Error",
        [RG_LANG_FR] = "Erreur carte SD",
        [RG_LANG_DE] = "SD-Card Fehler!",
        [RG_LANG_ZH] = "\x53\x44\x20\xE5\x8D\xA1\xE9\x94\x99\xE8\xAF\xAF",
    },
    {
        [RG_LANG_EN] = "Storage mount failed.\nMake sure the card is FAT32.",
        [RG_LANG_FR] = "Erreur carte SD.\nLa carte est bien en FAT32 ?",
        [RG_LANG_DE] = "Speicherbereitstellung fehlgeschlagen.\nStellen Sie sicher, dass die Karte FAT32 ist.",
    },
    // end of main.c


    // applications.c
    {
        [RG_LANG_EN] = "Scanning %s %d/%d",
        [RG_LANG_FR] = "Scan %s %d/%d",
        [RG_LANG_DE] = "Scannen %s %d/%d",
        [RG_LANG_ZH] = "\xE6\x89\xAB\xE6\x8F\x8F\x20\x25\x73\x20\x25\x64\x2F\x25\x64",
    },
    // message when no rom
    {
        [RG_LANG_EN] = "Welcome to Retro-Go!",
        [RG_LANG_FR] = "Bienvenue sur Retro-Go!",
        [RG_LANG_DE] = "Wilkommen bei Retro-Go!",
        [RG_LANG_ZH] = "\xE6\xAC\xA2\xE8\xBF\x8E\xE4\xBD\xBF\xE7\x94\xA8\x20\x52\x65\x74\x72\x6F\x2D\x47\x6F\x21",
    },
    {
        [RG_LANG_EN] = "Place roms in folder: %s",
        [RG_LANG_FR] = "Placer les ROMS dans le dossier: %s",
        [RG_LANG_DE] = "Lege ROMs in den Ordner: %s",
        [RG_LANG_ZH] = "\xE8\xAF\xB7\xE6\x8A\x8A\x20\x52\x4F\x4D\x20\xE6\x94\xBE\xE5\x85\xA5\xE6\x96\x87\xE4\xBB\xB6\xE5\xA4\xB9\x3A\x20\x25\x73",
    },
    {
        [RG_LANG_EN] = "With file extension: %s",
        [RG_LANG_FR] = "Avec l'extension: %s",
        [RG_LANG_DE] = "Mit Dateiendung: %s",
        [RG_LANG_ZH] = "\xE6\x96\x87\xE4\xBB\xB6\xE6\x89\xA9\xE5\xB1\x95\xE5\x90\x8D\x3A\x20\x25\x73",
    },
    {
        [RG_LANG_EN] = "You can hide this tab in the menu",
        [RG_LANG_FR] = "Vous pouvez cacher cet onglet dans le menu",
        [RG_LANG_DE] = "Sie können diesen Reiter im Menü ausblenden",
        [RG_LANG_ZH] = "\xE5\x8F\xAF\xE5\x9C\xA8\xE8\x8F\x9C\xE5\x8D\x95\xE4\xB8\xAD\xE9\x9A\x90\xE8\x97\x8F\xE6\xAD\xA4\xE6\xA0\x87\xE7\xAD\xBE",
    },
    {
        [RG_LANG_EN] = "You have no %s games",
        [RG_LANG_FR] = "Vous n'avez pas de jeux %s",
        [RG_LANG_DE] = "Du hast keine %s Spiele",
        [RG_LANG_ZH] = "\xE6\xB2\xA1\xE6\x9C\x89\x20\x25\x73\x20\xE6\xB8\xB8\xE6\x88\x8F",
    },
    {
        [RG_LANG_EN] = "File not found",
        [RG_LANG_FR] = "Fichier non présent",
        [RG_LANG_DE] = "Datei nicht gefunden",
        [RG_LANG_ZH] = "\xE6\x89\xBE\xE4\xB8\x8D\xE5\x88\xB0\xE6\x96\x87\xE4\xBB\xB6",
    },

    // rom options
    {
        [RG_LANG_EN] = "Name",
        [RG_LANG_FR] = "Nom",
        [RG_LANG_DE] = "Name",
        [RG_LANG_ZH] = "\xE5\x90\x8D\xE7\xA7\xB0",
    },
    {
        [RG_LANG_EN] = "Folder",
        [RG_LANG_FR] = "Dossier",
        [RG_LANG_DE] = "Ordner",
        [RG_LANG_ZH] = "\xE6\x96\x87\xE4\xBB\xB6\xE5\xA4\xB9",
    },
    {
        [RG_LANG_EN] = "Size",
        [RG_LANG_FR] = "Taille",
        [RG_LANG_DE] = "Größe",
        [RG_LANG_ZH] = "\xE5\xA4\xA7\xE5\xB0\x8F",
    },
    {
        [RG_LANG_EN] = "CRC32",
        [RG_LANG_FR] = "CRC32",
        [RG_LANG_DE] = "CRC32",
        [RG_LANG_ZH] = "\x43\x52\x43\x33\x32",
    },
    {
        [RG_LANG_EN] = "Delete file",
        [RG_LANG_FR] = "Supprimer fichier",
        [RG_LANG_DE] = "Datei löschen",
        [RG_LANG_ZH] = "\xE5\x88\xA0\xE9\x99\xA4\xE6\x96\x87\xE4\xBB\xB6",
    },
    {
        [RG_LANG_EN] = "Close",
        [RG_LANG_FR] = "Fermer",
        [RG_LANG_DE] = "Schließen",
        [RG_LANG_ZH] = "\xE5\x85\xB3\xE9\x97\xAD",
    },
    {
        [RG_LANG_EN] = "File properties",
        [RG_LANG_FR] = "Propriétés fichier",
        [RG_LANG_DE] = "Dateieigenschaften",
        [RG_LANG_ZH] = "\xE6\x96\x87\xE4\xBB\xB6\xE5\xB1\x9E\xE6\x80\xA7",
    },
    {
        [RG_LANG_EN] = "Delete selected file?",
        [RG_LANG_FR] = "Supprimer fichier?",
        [RG_LANG_DE] = "Ausgewählte Datei löschen?",
        [RG_LANG_ZH] = "\xE5\x88\xA0\xE9\x99\xA4\xE9\x80\x89\xE4\xB8\xAD\xE6\x96\x87\xE4\xBB\xB6\x3F",
    },


    // in-game menu
    {
        [RG_LANG_EN] = "Resume game",
        [RG_LANG_FR] = "Reprendre partie",
        [RG_LANG_DE] = "Fortsetzen",
        [RG_LANG_ZH] = "\xE7\xBB\xA7\xE7\xBB\xAD\xE6\xB8\xB8\xE6\x88\x8F",
    },
    {
        [RG_LANG_EN] = "New game",
        [RG_LANG_FR] = "Nouvelle partie",
        [RG_LANG_DE] = "Neues Spiel",
        [RG_LANG_ZH] = "\xE6\x96\xB0\xE6\xB8\xB8\xE6\x88\x8F",
    },
    {
        [RG_LANG_EN] = "Del favorite",
        [RG_LANG_FR] = "supp favori",
        [RG_LANG_DE] = "Favorit löschen",
        [RG_LANG_ZH] = "\xE5\x8F\x96\xE6\xB6\x88\xE6\x94\xB6\xE8\x97\x8F",
    },
    {
        [RG_LANG_EN] = "Add favorite",
        [RG_LANG_FR] = "Ajouter favori",
        [RG_LANG_DE] = "Favorit hinzufügen",
        [RG_LANG_ZH] = "\xE6\xB7\xBB\xE5\x8A\xA0\xE6\x94\xB6\xE8\x97\x8F",
    },
    {
        [RG_LANG_EN] = "Delete save",
        [RG_LANG_FR] = "Supp sauvegarde",
        [RG_LANG_DE] = "Speicherstand löschen?",
        [RG_LANG_ZH] = "\xE5\x88\xA0\xE9\x99\xA4\xE5\xAD\x98\xE6\xA1\xA3",
    },
    {
        [RG_LANG_EN] = "Properties",
        [RG_LANG_FR] = "Propriétés",
        [RG_LANG_DE] = "Eigenschaften",
        [RG_LANG_ZH] = "\xE5\xB1\x9E\xE6\x80\xA7",
    },
    {
        [RG_LANG_EN] = "Resume",
        [RG_LANG_FR] = "Reprendre",
        [RG_LANG_DE] = "Fortsetzen",
        [RG_LANG_ZH] = "\xE7\xBB\xA7\xE7\xBB\xAD",
    },
    {
        [RG_LANG_EN] = "Delete save?",
        [RG_LANG_FR] = "Supp sauvegarde?",
        [RG_LANG_DE] = "Speicherstand löschen?",
        [RG_LANG_ZH] = "\xE5\x88\xA0\xE9\x99\xA4\xE5\xAD\x98\xE6\xA1\xA3\x3F",
    },
    {
        [RG_LANG_EN] = "Delete sram file?",
        [RG_LANG_FR] = "Supp fichier SRAM?",
        [RG_LANG_DE] = "SRAM-Datei löschen?",
        [RG_LANG_ZH] = "\xE5\x88\xA0\xE9\x99\xA4\x20\x53\x52\x41\x4D\x20\xE6\x96\x87\xE4\xBB\xB6\x3F",
    },
    // end of applications.c


    // rg_system.c
    {
        [RG_LANG_EN] = "App unresponsive... Hold MENU to quit!",
        [RG_LANG_FR] = "Plantage... MENU pour quitter",
        [RG_LANG_DE] = "App reagiert nicht... Halte MENU zum Beenden",
        [RG_LANG_ZH] = "\xE7\xA8\x8B\xE5\xBA\x8F\xE6\x97\xA0\xE5\x93\x8D\xE5\xBA\x94\x2E\x2E\x2E\xE6\x8C\x89\xE4\xBD\x8F\xE8\x8F\x9C\xE5\x8D\x95\xE9\x80\x80\xE5\x87\xBA\x21",
    },
    {
        [RG_LANG_EN] = "Reset all settings",
        [RG_LANG_FR] = "Reset paramètres",
        [RG_LANG_DE] = "Einstellungen zurücksetzen",
        [RG_LANG_ZH] = "\xE9\x87\x8D\xE7\xBD\xAE\xE6\x89\x80\xE6\x9C\x89\xE8\xAE\xBE\xE7\xBD\xAE",
    },
    {
        [RG_LANG_EN] = "Reboot to factory ",
        [RG_LANG_FR] = "Redémarrer usine",
        [RG_LANG_DE] = "Neustart mit Werkseinstellungen",
        [RG_LANG_ZH] = "\xE9\x87\x8D\xE5\x90\xAF\xE5\x88\xB0\xE5\x87\xBA\xE5\x8E\x82\xE6\xA8\xA1\xE5\xBC\x8F",
    },
    {
        [RG_LANG_EN] = "Reboot to launcher",
        [RG_LANG_FR] = "Relancer launcher",
        [RG_LANG_DE] = "Neustart zum Launcher",
        [RG_LANG_ZH] = "\xE9\x87\x8D\xE5\x90\xAF\xE5\x88\xB0\xE5\x90\xAF\xE5\x8A\xA8\xE5\x99\xA8",
    },
    {
        [RG_LANG_EN] = "Recovery mode",
        [RG_LANG_FR] = "Mode de récupération",
        [RG_LANG_DE] = "Wiederherstellungsmodus",
        [RG_LANG_ZH] = "\xE6\x81\xA2\xE5\xA4\x8D\xE6\xA8\xA1\xE5\xBC\x8F",
    },
    {
        [RG_LANG_EN] = "System Panic!",
        [RG_LANG_FR] = "Plantage système!",
        [RG_LANG_DE] = "Systempanik!",
        [RG_LANG_ZH] = "\xE7\xB3\xBB\xE7\xBB\x9F\xE5\xB4\xA9\xE6\xBA\x83\x21",
    },
    {
        [RG_LANG_EN] = "Save failed",
        [RG_LANG_FR] = "Erreur sauvegarde",
        [RG_LANG_DE] = "Speichern fehlgeschlagen",
        [RG_LANG_ZH] = "\xE4\xBF\x9D\xE5\xAD\x98\xE5\xA4\xB1\xE8\xB4\xA5",
    },
    // end of rg_system.c
};
