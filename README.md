# Switch 2
Oprogramowanie włącznika światła automatycznego domu.

### Budowa włącznika
Całość zbudowana w oparciu o ESP8266 wraz z modułem RTC DS1307 oraz APDS-9960. Całości dopełnia 2-kanałowy przekaźnik SSR.

### Możliwości
Łączność z włącznikiem odbywa się przez sieć Wi-Fi.
Dane dostępowe do routera przechowywane są wraz z innymi informacjami w pamięci flash.
W przypadku braku informacji o sieci, urządzenie aktywuje wyszukiwania routera z wykorzystaniem funkcji WPS.

Włącznik automatycznie łączy się z zaprogramowaną siecią Wi-Fi w przypadku utraty połączenia.

Zawiera czujnik gestów wykorzystywany do sterowania oświetleniem bez udziału dedykowanej aplikacji. Gestem można również opuścić lub podnieść wszystkie rolety systemu iDom podłączonych do tej samej sieci Wi-Fi.

Zegar czasu rzeczywistego wykorzystywany jest przez funkcję ustawień automatycznych.
Ustawienia automatyczne obejmują włączanie i wyłączanie światła o wybranej godzinie oraz włączanie po zapadnięciu zmroku i wyłączanie o świcie. Powtarzalność obejmuje okres jednego tygodnia, a ustawienia nie są ograniczone ilościowo. W celu zminimalizowania objętości wykorzystany został zapis tożsamy ze zmienną boolean, czyli dopiero wystąpienie znaku wskazuje na włączoną funkcję.

* '4' wszystkie światła, którymi steruje włącznik - występuje tylko w zapisie aplikacji w celu zminimalizowania ilości przesyłanych danych
* '1', '2', '3' numer światła, którym steruje włącznik
* 'w' cały tydzień - występuje tylko w zapisie aplikacji w celu zminimalizowania ilości przesyłanych danych
* 'o' poniedziałek, 'u' wtorek, 'e' środa, 'h' czwartek, 'r' piątek, 'a' sobota, 's' niedziela
* 'n' włącz po zmroku / wyłącz o świcie
* '_' włącz o godzinie - jeśli znak występuje w zapisie, przed nim znajduje się godzina w zapisie czasu uniksowego
* '-' wyłącz o godzinie - jeśli występuje w zapisie, po nim znajduje się godzina w zapisie czasu uniksowego
* '/' wyłącz ustawienie - obecność znaku wskazuje, że ustawienie będzie ignorowane

Przykład zapisu trzech ustawień automatycznych: 1140_12w-420,4asn,/1ouehrn-300

### Sterowanie
Sterowanie włącznikiem odbywa się poprzez wykorzystanie metod dostępnych w protokole HTTP. Sterować można z przeglądarki lub dedykowanej aplikacji.

* "/hello" - Handshake wykorzystywany przez dedykowaną aplikację, służy do potwierdzenia tożsamości oraz przesłaniu wszystkich parametrów pracy włącznika.

* "/set" - Pod ten adres przesyłane są ustawienia dla włącznika, dane przesyłane w formacie JSON. Ustawić można strefę czasową ("offset"), czas RTC ("time"), ustawienia automatyczne ("smart"), włączyć lub wyłączyć światła ("val").

* "/state" - Służy do regularnego odpytywania włącznika o jego podstawowy stan, włączone światła.

* "/basicdata" - Służy innym urządzeniom systemu iDom do samokontroli. Jeśli któreś urządzenie po uruchomieniu nie pamięta aktualnej godziny lub nie posiada czujnika światła, ta funkcja zwraca aktualną godzinę i dane z czujnika.

* "/wifisettings" - Ten adres służy do usunięcia danych dostępowych do routera.

* "/log" - Pod tym adresem znajduje się dziennik aktywności urządzenia.
