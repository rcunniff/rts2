<?php
	require_once 'fitsdb.php';
	
function get_obj ()
{
	$rows = @ file ('http://archive.eso.org/skycat/servers/sim-server?&o=' 
		. str_replace ('+', '%20', urlencode ($_REQUEST['obj_name'])));
	if (!$rows[2])
	{
		return false;
	}
	$pompole = explode ("\t", $rows[2]);
	$jmeno = trim ($pompole[0]);
	$ra = trim ($pompole[1]);
	$dec = trim ($pompole[2]);

	$query = 'images.php?ra=' . urlencode($ra) . '&dec=' . urlencode($dec)
		. '&date_from=&date_to=&camera_name=&mount_name=&tar_id=';

	header('Location: '.$query);
	return true;
}

	if ( array_key_exists ('obj_name', $_REQUEST))
	{
		$ret = get_obj ();
	}
	if ( !$ret )
	{
		hlavicka ('Object search', 'Object search', ' ', 'ok');
		
		if ( array_key_exists ('obj_name', $_REQUEST))
		{
			echo '<div class=\'error\'>Cannot resolve object: "' . $_REQUEST['obj_name'] . '".</div>';
		}

		echo <<<EOF
<b>Object search</b><br/>
<p>Please fill in object name (e.g. M31). Object names will be resolved using 
<a href='http://simbad.u-strasbg.fr/'>Simbad</a>
application gateway (which runs on <a href='http://archive.eso.org'>archive.eso.org</a> server).</p>

<form action='obj.php'>
	<input type='text' name='obj_name'/>
	<br/>
	<input type='submit' value='Get object coordinates'/>
</form>
EOF;
		konec ();
	}
?>
