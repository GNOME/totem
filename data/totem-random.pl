#!/usr/bin/perl

# fisher_yates_shuffle( \@array ) : 
# generate a random permutation of @array in place
sub fisher_yates_shuffle {
	my $array = shift;
	my $i = @$array;
	while ($i--) {
		my $j = int rand ($i+1);
		next if $i == $j;
		@$array[$i,$j] = @$array[$j,$i];
	}
}

fisher_yates_shuffle(\@ARGV);

my $command="totem --enqueue ".join(" ", @ARGV);
#print $command;
exec $command;

