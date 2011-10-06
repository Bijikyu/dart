#!/usr/bin/perl

use Getopt::Long;
use Time::HiRes qw(usleep);

use Stockholm;
use Stockholm::Database;

# Options
my $delay = .01;

my $usage = "";
$usage .= "$0 -- page through Stockholm alignments in a database\n";
$usage .= "\n";
$usage .= "Usage: cat <filename> | $0 [-delay <seconds>] [filename(s)]\n";
$usage .= "\n";
$usage .= "Default delay is $delay seconds\n";
$usage .= "\n";

GetOptions ("delay=i" => \$delay) or die $usage;

# Get terminal size
my $screenColumns = (`tput cols` + 0) || 80;

# Process
unless (@ARGV) {
    @ARGV = qw(-);
    warn "[waiting for alignments on standard input]\n";
}

for my $filename (@ARGV) {
    my $db = Stockholm::Database->from_file ($filename);
    for my $stock (@$db) {
	# prepare text
	my $stockText = $stock->to_string ($screenColumns);

	# clear screen
	system "clear";

	# print
	print $stockText;

	# delay
	usleep ($delay * 1e6);
    }
}
