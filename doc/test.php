#!/usr/bin/php -nf
<?php
print("Content-Type: text/html\n\n");
print("<!DOCTYPE html>\n"
     ."<html>\n"
     ."  <head>\n"
     ."    <title>PHP script CGI test page</title>\n"
     ."  </head>\n"
     ."  <body>\n"
     ."    <h1>PHP script CGI test page</h1>\n"
     ."    <pre><kbd>ps ax</kbd>\n");
$fp = popen("ps ax", "r");
while ($line = fgets($fp, 1024))
{
  print(htmlspecialchars($line));
}
print("</pre>\n"
     ."  </body>\n"
     ."</html>\n");
?>
