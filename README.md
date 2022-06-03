# Włącznik światła
Oprogramowanie włącznika światła automatycznego domu.

### Budowa włącznika
Oprogramowanie jest przystosowane do włączników dotykowych firmy Tuya i jej klonów.

### Możliwości
Łączność z włącznikiem odbywa się przez sieć Wi-Fi.
Włącznik automatycznie łączy się z zaprogramowaną siecią Wi-Fi w przypadku utraty połączenia.
W przypadku braku informacji o sieci lub braku łączności z zapamiętaną siecią, urządzenie aktywuje wyszukiwania routera z wykorzystaniem funkcji WPS.

Dane dostępowe do routera przechowywane są wraz z innymi informacjami w pamięci flash.

Włącznik ma możliwość reagowania na wschód i zachód słońca (po podaniu lokalizacji) oraz zmierzch i świt (dane otrzymane z czujnika światła rolety).

Programowy zegar czasu rzeczywistego wykorzystywany jest przez funkcję ustawień automatycznych. Czas jest synchronizowany z Internetu.

Ustawienia automatyczne obejmują włączenie lub wyłączenia światła o wybranej godzinie, reagowanie na zmierzch, świt, zachód czy wschód słońca.
Włącznik oferuje również ustawienie ograniczeń wywołania wyzwalaczy ustawień automatycznych, jak np. "Światło 1 musi być włączone" lub "Światło 1 musi być wyłączone".
Możliwe jest również, ustawienie wymogu spełnienia kilku warunków jednocześnie, np. "Włącz o świcie, ale nie wcześniej niż o 6:00".
Powtarzalność ustawień automatycznych obejmuje okres jednego tygodnia, a ustawienia nie są ograniczone ilościowo.
W celu zminimalizowania objętości wykorzystany został zapis tożsamy ze zmienną boolean, czyli dopiero wystąpienie znaku wskazuje na włączoną funkcję.

* '1', '2', '3' numer światła, którym steruje urządzenie
* '4' wszystkie światła, którymi steruje urządzenie
* 'w' cały tydzień - występuje tylko w zapisie aplikacji w celu zminimalizowania ilości przesyłanych danych
* 'o' poniedziałek, 'u' wtorek, 'e' środa, 'h' czwartek, 'r' piątek, 'a' sobota, 's' niedziela
* 'n' o zachodzie słońca
* 'd' o wschodzie słońca
* '<' po zmroku
* '>' po świcie
* 'z' reaguj na zachmurzenie (po zmroku oraz po świcie)
* '_' o godzinie - jeśli znak występuje w zapisie, przed nim znajduje się godzina w zapisie czasu uniksowego
* '-' wyłącz o godzinie - jeśli występuje w zapisie, po nim znajduje się godzina w zapisie czasu uniksowego
* '6', '7', '8', '9' - numer ograniczenia uruchomienia wyzwalacza; Musi być włączone: światło 1 ('6'), światło 2 ('7'); Musi być wyłączone: światło 1 ('8'), światło 2 ('9')
* '/' wyłącz ustawienie - obecność znaku wskazuje, że ustawienie będzie ignorowane
* '&' wszystkie wyzwalacze muszą zostać spełnione by wykonać akcje
* cyfra bezpośrednio przed 'l', ale po znaku "_" (jeśli występuje) oznacza stan włącznika, 0 lub 1

Obecność znaku 'l' wskazuje, że ustawienie dotyczy włącznika światła.

Przykład zapisu ustawień automatycznych: l2w>-1390,l28wn<

### Sterowanie
Sterowanie urządzeniem odbywa się poprzez wykorzystanie metod dostępnych w protokole HTTP. Sterować można z przeglądarki lub dedykowanej aplikacji.

* "/hello" - Handshake wykorzystywany przez dedykowaną aplikację, służy do potwierdzenia tożsamości oraz przesłaniu wszystkich parametrów pracy urządzenia.

* "/set" - Pod ten adres przesyłane są ustawienia dla włącznika, dane przesyłane w formacie JSON. Ustawić można m.in. strefę czasową ("offset"), czas RTC ("time"), ustawienia automatyczne ("smart"), włączyć lub wyłączyć światła ("val").

* "/state" - Służy do regularnego odpytywania urządzenia o jego stan włączone światła.

* "/basicdata" - Służy innym urządzeniom systemu iDom do samokontroli, urządzenia po uruchomieniu odpytują się wzajemnie m.in. o aktualny czas lub dane z czujników.

* "/log" - Pod tym adresem znajduje się dziennik aktywności urządzenia (domyślnie wyłączony).
