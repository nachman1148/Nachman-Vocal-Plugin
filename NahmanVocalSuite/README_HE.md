# NAHMAN VOCAL SUITE — חבילת בנייה ל-Windows

**סטטוס: קוד מקור / אב-טיפוס. לא מוצר בינארי מאומת.**

הפרויקט מכוון ליצירת VST3 ויישום Standalone באמצעות JUCE 8.0.6 ו-CMake.

## מה יש בפרויקט
- ממשק עברי עם כיתוב מקצועי באנגלית.
- מצב זיהוי/ניתוח בסיסי ומצב ידני.
- שרשרת DSP בסיסית: EQ, Compressor, Gate, Saturation, Reverb ו-Delay.
- מודול ניסיוני לתיקון גובה צליל מונופוני (Auto-Tune בסיסי).
- בחירת תו יסוד, סולם כרומטי/מז'ור/מינור, מהירות וכמות תיקון.

## דרישות לבנייה
- Windows 10/11 64-bit.
- Visual Studio 2022 עם workload בשם **Desktop development with C++**.
- CMake 3.22 ומעלה.
- Git for Windows.
- חיבור אינטרנט להורדת JUCE בפעם הראשונה.

## בנייה
1. חלץ את כל קובצי ה-ZIP לתיקייה רגילה.
2. פתח PowerShell בתוך התיקייה `NahmanVocalSuite`.
3. הרץ:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\BUILD_WINDOWS.ps1
```

4. אם הבנייה מצליחה, חפש את התוצרים בתיקיות:
   - `build-windows\NahmanVocalSuite_artefacts\Release\VST3`
   - `build-windows\NahmanVocalSuite_artefacts\Release\Standalone`

5. התקן את קובץ ה-VST3 בתיקיית VST3 המקובלת של Windows, ואז בצע Rescan ב-Cubase.

## מגבלות חשובות
- לא צורף קובץ VST3 או EXE מוכן. יש לבנות אותם ב-Windows.
- הקוד לא עבר קומפילציה או בדיקות QA בסביבת יצור.
- תיקון הגובה הנוכחי הוא אלגוריתם ניסיוני, ואינו שקול למנוע Auto-Tune מסחרי: ייתכנו ארטיפקטים, מעקב לא יציב ועיכוב. יש לבדוק על עותק של ההקלטה לפני שימוש בפרויקט חשוב.
- ניתוח הסגנון הוא היוריסטי המבוסס על מאפייני אות, לא מודל AI מאומן ולא זיהוי ז'אנר אמין.
- לפני הפצה מסחרית יש לבדוק את תנאי הרישוי של JUCE ואת כל התלויות, לבצע בדיקות אודיו, latency, CPU, mono/stereo, שמירת פרמטרים ויציבות ב-Cubase.

## פרטי בנייה
- Framework: JUCE 8.0.6
- Build system: CMake
- Targets: VST3, Standalone
- Language: C++17

## בנייה דרך GitHub Actions (בלי להתקין כלי פיתוח במחשב)
1. חלץ את ה-ZIP.
2. העלה ל-GitHub את התיקייה `NahmanVocalSuite` ואת התיקייה `.github` שנמצאת בחבילה, אל שורש המאגר שלך. אין למחוק את קובצי האתר הקיימים.
3. המתן לסיום הפעולה `Build NAHMAN VOCAL SUITE VST3` בלשונית Actions.
4. כשהפעולה מסתיימת בהצלחה, פתח אותה והורד את Artifact בשם `NahmanVocalSuite-Windows-64bit`.
5. חלץ את ה-Artifact. התקן את תיקיית `NAHMAN VOCAL SUITE.vst3` בתיקיית VST3 של Windows, למשל:
   `%CommonProgramFiles%\\VST3`
6. פתח את Cubase ובצע Rescan ל-VST3.

אם הפעולה נכשלת, יש לפתוח את הפעולה בלשונית Actions ולהעתיק את שגיאת הבנייה. אין להתקין קובץ VST3 אם לא נוצר Artifact בהצלחה.

**חשוב:** GitHub Actions מקמפל את קוד המקור על Windows בענן. החבילה עדיין אינה כוללת VST3 מוכן. הצלחת קומפילציה אינה תחליף לבדיקות שמע ויציבות ב-Cubase.
