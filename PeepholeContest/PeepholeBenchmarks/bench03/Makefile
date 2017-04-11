all: clean
	$(PEEPDIR)/joosc *.java *.joos
	make -C lib

opt: clean
	$(PEEPDIR)/joosc -O *.java *.joos
	make -C lib

java:
	javac *.java
	make -C lib

clean:	
	rm -rf *.class *.j *~ newout
	make -C lib clean

run:
	java -classpath "../../jooslib.jar:.:lib/" Main < in1

diff:
	java -classpath "../../jooslib.jar:lib/:." Main < in1 > newout; diff out1 newout
