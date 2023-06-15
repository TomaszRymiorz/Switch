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

* '1', '2', '3' przed symbolem "|" lub "&" (jeśli nie występuje "|") oznacza numer światła, którym steruje urządzenie
* '4' to wszystkie światła, którymi steruje urządzenie
* 'o' poniedziałek, 'u' wtorek, 'e' środa, 'h' czwartek, 'r' piątek, 'a' sobota, 's' niedziela
* Brak wskazania dnia wygodnia oznacza, że ustawienie obejmuje cały tydzień
* 'n' wyzwalacz o zachodzie słońca.
* 'd' wyzwalacz o wschodzie słońca
* '<' wyzwalacz o zmroku
* '>' wyzwalacz o świcie
* 'z' wyzwalacz reaguj na zachmurzenie (po zmroku oraz po świcie)
* Każdy z powyższych wyzwalaczy może zawierać dodatkowe parametry zawarte w nawiasach, jak opóźnienie czasowe lub własne ustawienie LDR.
* 'l()', 'b()', 't()', 'c()' to wyzwalacze związane bezpośrednio z urządzeniem.
* 'l()' włączenie/wyłączenie światła
* 'b()', 'c()' pozycja rolety lub okna
* 't()' osiągnięcie określonej temperatury na termostacie
* '_' o godzinie - jeśli znak występuje w zapisie, przed nim znajduje się godzina w zapisie czasu uniksowego
* 'h(-1;-1)' między godzinami, jeśli obie cyfry są różne od "-1" lub po godzinie, przed godziną. "-1" oznacza, że nie ma wskazanej godziny
* '/' wyłącz ustawienie - obecność znaku wskazuje, że ustawienie będzie ignorowane
* '&' wszystkie wyzwalacze muszą zostać spełnione by wykonać akcje
* cyfra między symbolami "|" i "|" (lub "&" jako drugi symbol, jeśli jest wskazanie na wszystkie wyzwalacze) oznacza akcje do wykonania
* Obecność znaku 'l' wskazuje, że ustawienie dotyczy włącznika światła.
* 'r()' i 'r2()' w nawiasach zawierają warunki, które muszą być spełnione w chwili aktywacji wyzwalacza, aby wykonać akcje
* 'r()' to wymaganie określonego stanu świateł, pozycji rolety, okna lub stanu czy temperatury termostatu
* 'r2()' wymaganie dotyczące pozycji słońca: wschód, zmierzch, świt, zmrok

### Sterowanie
Sterowanie urządzeniem odbywa się poprzez wykorzystanie metod dostępnych w protokole HTTP. Sterować można z przeglądarki lub dedykowanej aplikacji.

* "/hello" - Handshake wykorzystywany przez dedykowaną aplikację, służy do potwierdzenia tożsamości oraz przesłaniu wszystkich parametrów pracy urządzenia.

* "/set" - Pod ten adres przesyłane są ustawienia dla włącznika, dane przesyłane w formacie JSON. Ustawić można m.in. strefę czasową ("offset"), czas RTC ("time"), ustawienia automatyczne ("smart"), włączyć lub wyłączyć światła ("val").

* "/state" - Służy do regularnego odpytywania urządzenia o jego stan włączone światła.

* "/basicdata" - Służy innym urządzeniom systemu iDom do samokontroli, urządzenia po uruchomieniu odpytują się wzajemnie m.in. o aktualny czas lub dane z czujników.

* "/log" - Pod tym adresem znajduje się dziennik aktywności urządzenia (domyślnie wyłączony).
