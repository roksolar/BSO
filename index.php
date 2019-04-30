<!DOCTYPE HTML>
<html>
<head>  
<script>




</script>
</head>
<body>
<p>Choose dates:</p>
<input type="text" name="datetimes" />
<div id="chartContainer" style="height: 370px; width: 100%;"></div>
<script src="https://canvasjs.com/assets/script/canvasjs.min.js"></script>
<script type="text/javascript" src="https://cdn.jsdelivr.net/jquery/latest/jquery.min.js"></script>
<script type="text/javascript" src="https://cdn.jsdelivr.net/momentjs/latest/moment.min.js"></script>
<script type="text/javascript" src="https://cdn.jsdelivr.net/npm/daterangepicker/daterangepicker.min.js"></script>
<link rel="stylesheet" type="text/css" href="https://cdn.jsdelivr.net/npm/daterangepicker/daterangepicker.css" />
</body>
</html>




<?php
  if (isset($_GET['date'])) {
	getData($_GET['start']/1000, $_GET['end']/1000);
  }
  if (!isset($_GET['date'])) {
	getData(-1, -1);
  }

    function getData($start, $end) {
	// Read from db
	$db = new SQLite3('/home/pi/BSO/sensordata.db');
	if($start == -1 || $end == -1){
		$results1 = $db->query("SELECT * FROM readings WHERE device='/BSO-1'");
		$results2 = $db->query("SELECT * FROM readings WHERE device='/BSO-2'");
	}else{
echo "<script> console.log('$start')</script>";
echo "<script> console.log('$end')</script>";
		$results1 = $db->query("SELECT * FROM readings WHERE device='/BSO-1' and currentdate > datetime($start, 'unixepoch', 'localtime') and currentdate < datetime($end, 'unixepoch', 'localtime')");
		$results2 = $db->query("SELECT * FROM readings WHERE device='/BSO-2' and currentdate > datetime($start, 'unixepoch', 'localtime') and currentdate < datetime($end, 'unixepoch', 'localtime')");
	}

	$min = 9999999999.0;
	// Find minimum value
	while ($row = $results1->fetchArray()) {
		if($row[1] < $min){
			$min = $row[1];	
		}
	}

	while ($row = $results2->fetchArray()) {
		if($row[1] < $min){
			$min = $row[1];	
		}
	}
	$min = $min *0.99999;
	echo "

	<script> var chart = new CanvasJS.Chart('chartContainer', {
		title: {
			text: 'Air pressure time plot'
		},
		axisX: {
			title:'Date and time'
		},
		axisY2: {
			title: 'Air pressure [Pa]',
			prefix: '',
			suffix: ' Pa',
			minimum:$min
		},
		toolTip: {
			shared: true
		},
		legend: {
			cursor: 'pointer',
			verticalAlign: 'top',
			horizontalAlign: 'center',
			dockInsidePlotArea: true,
			itemclick: toogleDataSeries
		},
		data: [{
			type:'spline',
			axisYType: 'secondary',
			name: 'BSO-1',
			showInLegend: true,
			markerSize: 0,
			yValueFormatString: '# Pa',
			dataPoints: [	";

			while ($row = $results1->fetchArray()) {
			    echo "{ x: new Date('" . $row[2] . "'), y: $row[1] },";
			}
	echo "]},{
			type:'spline',
			axisYType: 'secondary',
			name: 'BSO-2',
			showInLegend: true,
			markerSize: 0,
			yValueFormatString: '# Pa',
			dataPoints: [	";

			while ($row = $results2->fetchArray()) {
			    echo "{ x: new Date('" . $row[2] . "'), y: $row[1] },";
			}
	echo "		]
		}]
	});
	chart.render();


	function toogleDataSeries(e){
		if (typeof(e.dataSeries.visible) === 'undefined' || e.dataSeries.visible) {
			e.dataSeries.visible = false;
		} else{
			e.dataSeries.visible = true;
		}
		chart.render();
	}

	 </script>";
}
?>


<script>
$(function() {
  $('input[name="datetimes"]').daterangepicker({
    timePicker: true,
    startDate: moment().startOf('hour'),
    endDate: moment().startOf('hour').add(32, 'hour'),
    locale: {
      format: 'DD.MM hh:mm A'
    }
  }, function(start, end, label) {
	window.location.href = "http://localhost/index.php?date=true&start="+start+"&end="+end;
  });
});

</script>

