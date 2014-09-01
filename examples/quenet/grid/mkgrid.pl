#!/usr/bin/perl

use strict;

my $n;
my $m;
if(scalar(@ARGV) == 2) {
  $n = shift @ARGV;
  $m = shift @ARGV;
} else { die "Usage: $0 N M  (N and M are dimensions of the grid)\n"; }

if($n < 1 || $m < 1) { die "invalid grid dimensions: ${n}x${m}\n"; }

my $total = $n*$m;
print "${total} 10\n";

for(my $i=0; $i<$n; $i++) {
  my $north = 0; if($i>0) { $north = 1; }
  my $south = 0; if($i<$n-1) { $south = 1; }
  for(my $j=0; $j<$m; $j++) {
    my $west = 0; if($j>0) { $west = 1; }
    my $east = 0; if($j<$m-1) { $east = 1; }
    my $id = getid($i,$j);
    my $conn = $north+$south+$west+$east;
    print "${id} FIFO 1.0 ${conn}";
    my $prob = 1.0/$conn;
    if($north > 0) { my $northid = getid($i-1,$j); print " $northid ${prob} 1.0"; }
    if($south > 0) { my $southid = getid($i+1,$j); print " $southid ${prob} 1.0"; }
    if($west > 0) { my $westid = getid($i,$j-1); print " $westid ${prob} 1.0"; }
    if($east > 0) { my $eastid = getid($i,$j+1); print " $eastid ${prob} 1.0"; }
    print "\n";
  }
}

sub getid {
  my $i = shift;
  my $j = shift;
  return $i*$m+$j;
}
