#include "BCC.h"
#include "BIPLAN.h"

BCC      bcc;
BIPLAN_Interpreter interpreter;

void error_callback(char *position, const char *string) {
  Serial.print("error: ");
  Serial.print(string);
  Serial.print(" ");
  Serial.print(*position);
  Serial.print(" at position ");
  Serial.println(position - interpreter.program_start);
};

char program[] =
"print \"X Drawer example \n Please input resolution between 3 and 9:\" \n\
$res = 0 \n\
while $res < 3 \n\
  if serialAvailable $res (serialRead - 48) endif \n\
next \n\
print \" \", $res--, \"\n\" \n\
for $y = 0 to $res \n\
  for $x = 0 to $res \n\
    if $x == $y || ($x + $y == $res) \n\
      print \"X\" \n\
    else \n\
      print \" \" \n\
    endif \n\
  next \n\
  print \"\n\" \n\
next \n\
restart\n";

void setup() {
  Serial.begin(115200);
  Serial.print(program);
  uint16_t length = strlen(program);
  Serial.println("--------------------------");
  Serial.print("Program length: ");
  Serial.print(length);
  Serial.println(" bytes");
  Serial.println();
  uint32_t time = millis();

  bcc.compile(program);

  interpreter.initialize(
    program,
    error_callback,
    &Serial,
    &Serial,
    &Serial
  );

  Serial.print(program);
  uint16_t new_length = strlen(program);
  Serial.println("--------------------------");
  Serial.print("Compilation duration: ");
  Serial.print(millis() - time);
  Serial.println(" milliseconds");
  Serial.print("BIPLAN program length: ");
  Serial.print(length);
  Serial.println(" bytes");
  Serial.print("BIPLAN compiled program length: ");
  Serial.print(new_length);
  Serial.println(" bytes");
  Serial.print("Reduction: ");
  Serial.print(100 - (new_length * 100.0) / length);
  Serial.println("%");
  Serial.println("Program output:");
  Serial.println();
}

/*---------------------------------------------------------------------------*/
void loop() {
  do {
    interpreter.run();
  } while(!interpreter.finished());
  while(true);
}
/*---------------------------------------------------------------------------*/
