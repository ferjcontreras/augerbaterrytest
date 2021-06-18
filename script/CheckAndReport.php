<?php


  $db = mysqli_connect("db.auger.org.ar", "checker", "Ch3ck3r.2018", "AugerChecker");


  $filename = "/home/fcontreras/BatteryTest/.running";
  $handle = fopen($filename, "r");
  $size = 2048;
  $idstart = 333; // id donde comienzan las baterias en AugerChecker tabla Monitor
  $index = 0;

  while($data = fgetcsv($handle,$size,";")) {
    $iter = $data[0];
    $serial = $data[1];
    $voltage = $data[2];
    $current = $data[3];
    $status = $data[4];
    $id = $idstart + $index;

    if ($status == "R") { // Battery is still running the test...
      $sqlqry = "UPDATE Monitor set Name = '$serial', Status = 0, Detail = 'V: $voltage - I: $current - sample: $iter', Ack = NULL WHERE ID = '$id';";
    } else if ($status == 'S') { // Battery was stopped
      $sqlqry = "UPDATE Monitor set Name = '$serial', Status = 2, Detail = 'V: $voltage - I: $current - sample: $iter'  WHERE ID = '$id';";
    }
    echo $sqlqry."\n";
    mysqli_query($db, $sqlqry);
    if (mysqli_errno($db)) {
      echo "Error: $sqlqry\n";
      die;
    }
    $index++;
  }
  $faltante = 8 - $index;
  for ($i=0; $i<$faltante; $i++) {
    $id = $idstart + $index;
    $sqlqry = "UPDATE Monitor set Name = 'Not Used', Status = 0, Detail = 'Battery not connected', Ack = NULL WHERE ID = '$id';";
    echo $sqlqry."\n";
    mysqli_query($db, $sqlqry);
    if (mysqli_errno($db)) {
      echo "Error: $sqlqry\n";
      die;
    }
    $index++;
  }




?>
