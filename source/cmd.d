module cmd;

import std.stdio : writeln;
import core.stdc.stdlib : exit;
import std.string;
import std.array;

void function(string[] args, int currarg)[string] cmd_options;
string output_file = "";
string source_file = "";

void init_options() {
    cmd_options = [
		"-h": function(string[] args, int currarg) {
			writeln("List of compiler options:");
            writeln("   -h - Get a list of compiler options.");
            writeln("   -o PATH - Specify the full path of the output file.");
			exit(0);
		},
        "-o": function(string[] args, int currarg) {
            output_file = args[currarg+1];
        }
	];
}

void input(string[] args) {
    init_options();
    if(args.length>1) {
        source_file = args[1];
        int i = 2;
        while(i<args.length) {
            if(args[i] in cmd_options) {
                if(args[i]=="-o") {
                    if(args.length > i+1) {
                        cmd_options["-o"](args,i);
                        i+=2;
                    }
                    else {
                        writeln("There are not enough arguments to set the -o option!");
                        exit(-1);
                    }
                }
                else {
                    cmd_options[args[i]](args,i);
                    i+=1;
                }
            }
            else {
                writeln("Unknown compiler option \""~args[i]~"\"!");
                exit(-1);
            }
        }
    }
    else {
        writeln("Error: the source file path was not found!");
        exit(-1);
    }
}