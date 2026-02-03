export = {}; -- create a new export object

-- return label
function export:label()
	return "Flare";
end

-- return category
function export:category()
	return "2D";
end

-- return argument list 
function export:options()
	groups = {};
	for a,actor in magpie.getactors() do
		for g,group in magpie.getgroups(actor) do
			table.insert(groups, group);
		end
	end
  	return {
		{"group", "Group", "choice", table.concat(groups, "|")},
  		{"toonz_output_file", "Output File", "output_file", "Text files (*.tls)\tAll files (*.*)"}
  	};
end

-- perform the export
function export:run(from, to, options)
	
	-- open output file
	fd = io.open(options.toonz_output_file, "wt");
	if (fd == nil) then
		return string.format("could not open '%s'", options.toonz_output_file);
	end
	
	-- write header line to file
	fd:write("Flare\n");
	
	-- create an array of all the poses that are being exported
	line = "";
	poses = magpie.getposes(options.group);
	for p,pose in poses do
		line = string.gsub(pose, "[^%.]+%.", "");
		fd:write(line, "\n");
		line = "";
	end

	-- write data to file
	for frame = from, to do
		line = "";

		line = frame + magpie.getframeoffset();

		k = nil;
		k = magpie.getgroupvalue(frame, options.group);
		if (k ~= nil) then
			k = string.gsub(k, "[^%.]+%.", ""); -- remove actor and group name from string
		end
		if (k == nil) then
			k = "<none>";
		end
		
		if (line ~= "") then
			line = line .. "|";
		end
		line = line .. k;

		if (line ~= "") then
			line = line .. "|";
		end

		comment = magpie.getframecomment(frame);
		if (comment ~= "") then
			is_empty = false;
		else
			comment = "<none>";
		end
		line = line .. comment;
		
		fd:write(line, "\n");
		
		-- update progress bar in main window		
		magpie.progress("Exporting...", (frame - from) / (to - from));
	end
	
	magpie.progress("", 0); -- close progress bar
	
	fd:close(); -- close output file
end
