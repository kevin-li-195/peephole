all: clean
	$(PEEPDIR)/joosc *.java

opt: clean
	$(PEEPDIR)/joosc -O *.java

java:
	javac *.java

clean:	
	rm -rf *.class *.j *~ newout

run:
	java -classpath "../../jooslib.jar:." GrueHunt < in1

diff:
	java -classpath "../../jooslib.jar:." GrueHunt < in1 > newout; tail -10 newout 
