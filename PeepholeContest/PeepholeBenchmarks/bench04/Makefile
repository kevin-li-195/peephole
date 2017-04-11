all: clean
	$(PEEPDIR)/joosc *.java 

opt: clean
	$(PEEPDIR)/joosc -O *.java 

java:
	javac *.java

clean:	
	rm -rf *.class *.j *~ newout

run:
	java -classpath "../../jooslib.jar:." Game < in1

diff:
	java -classpath "../../jooslib.jar:." Game < in1 > newout; diff out1 newout
