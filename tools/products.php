#!/usr/bin/php -f
<?php

$fp     = popen("zgrep '^\\*Product:' /Library/Printers/PPDs/Contents/Resources/*.gz", "r");
$files  = array();
$maxlen = 0;

while ($line = fgets($fp, 1024))
{
  $data = explode(":", $line);
  if (array_key_exists($data[0], $files))
    $files[$data[0]] ++;
  else
    $files[$data[0]] = 1;

  $data = explode("\"", $line);
  if (strlen($data[1]) > $maxlen)
    $maxlen = strlen($data[1]);
}

pclose($fp);

arsort($files);

$current_count = 0;
$current_files = 0;

foreach ($files as $file => $count)
{
  if ($current_count == 0)
    print(basename($file) . "  => $count products\n");

  if ($count != $current_count)
  {
    if ($current_count != 0)
      print("$current_files PPDs with $current_count products.\n");

    $current_count = $count;
    $current_files = 1;
  }
  else
    $current_files ++;
}

if ($current_count != 0)
  print("$current_files PPDs with $current_count products.\n");

print("Maximum length of Product string: $maxlen\n");

?>
