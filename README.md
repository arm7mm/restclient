# restclient
Simple rest client
                                
                                Описание программы.

 Программа (далее клиент) отправляет POST запрос серверу localhost:8000, передавая текст json с методом 
и параметрами, которые читаются из файла input в папке с программой. Формат файла следующий: 
 первый символ '"', затем имя метода, далее снова '"' (то есть это json строка). Далее могут следовать 
предусмотренные форматом json разделители. После параметры в виде json выражения. Пример содержимого
входного файла может быть таким:

"systemInfo"{"id":0, "jsonrpc":"2.0","method":"systemInfo","params":[null, false, true, []]}

 В случае, если код ответа сервера предполагает перенаправление, клиент посылает POST запрос по 
указанному в заголовке Location адресу. Всего может быть не более 50 перенаправлений.
Если код ответа не предполагает перенаправление и не указывает на ошибку, в теле ответа ищется 
контент, который, затем, проверяется на соответствие формату json. В случае успеха контент 
сохраняется в файле output.

                                Сборка программы.

Сборка может быть осуществлена путём выполнения 
              ./build.sh 
                 или 
clang -std=c99 src/strcmpr.c src/stack.c src/url.c src/jsonutf.c src/restclient.c -o restclient
                 или 
gcc -std=c99 src/strcmpr.c src/stack.c src/url.c src/jsonutf.c src/restclient.c -o restclient

                                Запуск
./restclient

(linux x86_64, clang++ 7.0.1)
