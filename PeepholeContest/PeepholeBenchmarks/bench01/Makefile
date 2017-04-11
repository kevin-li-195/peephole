all: clean
	$(PEEPDIR)/joosc *.java

opt: clean
	$(PEEPDIR)/joosc -O *.java

java:
	javac *.java

clean:	
	rm -rf *.class *.j *~ newout

run:
	java -cp "../../jooslib.jar:." Benchmark < in1

diff:
	java -cp "../../jooslib.jar:." Benchmark < in1 > newout; diff out1 newout
