// Stepper
const int STEPPER_PINS[] = {8, 9, 10, 11};
const int STEP_DELAY_US        = 4166;
const int SHARP_DIR            = 1;
const int STEPS_PER_CORRECTION = 200;

// Microphone
const int MIC_PIN     = A0;
const int SAMPLE_RATE = 9000;
const int BUFFER_SIZE = 512;
int samples[BUFFER_SIZE];
const int ATTACK_SKIP = 1800;

// RGB LED
const int LED_R = 5;
const int LED_G = 6;
const int LED_B = 7;

// Noise gate
const long NOISE_FLOOR_RMS2 = 500;

// Frequency averaging
const int AVG_SIZE = 6;
float freqHistory[AVG_SIZE];
int   avgIndex    = 0;
int   avgCount    = 0;

// New frequency detected
void pushFreq(float f) {
  freqHistory[avgIndex] = f;
  avgIndex = (avgIndex + 1) % AVG_SIZE;
  if (avgCount < AVG_SIZE) avgCount++;
}

// Returns the average of whatever is currently in the history buffer
float avgFreq() {
  float sum = 0;
  for (int i = 0; i < avgCount; i++) sum += freqHistory[i];
  return sum / avgCount;
}

// Reset when a new string is identified
void resetAvg() {
  avgCount = 0;
  avgIndex = 0;
}

// String table
struct GuitarString {
  const char* name;
  float       freq;
};

const GuitarString STRINGS[] = {
  { "E2", 84.11  },
  { "A2", 111.11 },
  { "D3", 147.54 },
  { "G3", 200.00 },
  { "B3", 250.00 },
  { "E4", 333.33 },
};
const int NUM_STRINGS = 6;

// Track which string was identified last loop
int lastDetected = -1;
const float TUNE_TOLERANCE = 0.5;

void setup() {
  Serial.begin(9600);
  for (int i = 0; i < 4; i++) pinMode(STEPPER_PINS[i], OUTPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  setLED(0, 0, 0);
}

// LED helper
void setLED(int r, int g, int b) {
  analogWrite(LED_R, r);
  analogWrite(LED_G, g);
  analogWrite(LED_B, b);
}

// Stepper helpers
void stepMotor(int dir) {
  static const int sequence[4][4] = {
    {1, 1, 0, 0},
    {0, 1, 1, 0},
    {0, 0, 1, 1},
    {1, 0, 0, 1}
  };
  static int currentStep = 0;
  currentStep = (currentStep + dir + 4) % 4;
  for (int j = 0; j < 4; j++)
    digitalWrite(STEPPER_PINS[j], sequence[currentStep][j]);
  delayMicroseconds(STEP_DELAY_US);
  for (int j = 0; j < 4; j++) digitalWrite(STEPPER_PINS[j], LOW);
}


void turnSteps(int numSteps, int dir) {
  for (int i = 0; i < numSteps; i++) stepMotor(dir);
}

// Sampling
void collectSamples() {
  unsigned long nextSample = micros();
  for (int i = 0; i < ATTACK_SKIP; i++) {
    while (micros() < nextSample);
    analogRead(MIC_PIN);
    nextSample += 1000000UL / SAMPLE_RATE;
  }
  for (int i = 0; i < BUFFER_SIZE; i++) {
    while (micros() < nextSample);
    samples[i] = analogRead(MIC_PIN);
    nextSample += 1000000UL / SAMPLE_RATE;
  }
}

// Pitch detection
float detectPitch() {
  long sum = 0;
  for (int i = 0; i < BUFFER_SIZE; i++) sum += samples[i];
  int dcOffset = sum / BUFFER_SIZE;
  for (int i = 0; i < BUFFER_SIZE; i++) samples[i] -= dcOffset;

  long power = 0;
  for (int i = 0; i < BUFFER_SIZE; i++) power += (long)samples[i] * samples[i];
  if (power / BUFFER_SIZE < NOISE_FLOOR_RMS2) return -1;

  int minLag = SAMPLE_RATE / 360;
  int maxLag = SAMPLE_RATE / 40;
  if (maxLag > BUFFER_SIZE / 2) maxLag = BUFFER_SIZE / 2;

  long r0 = 0;
  for (int i = 0; i < BUFFER_SIZE - maxLag; i++)
    r0 += (long)samples[i] * samples[i];

  long bestMerit = -1;
  int  bestLag   = -1;

  for (int lag = minLag; lag <= maxLag; lag++) {
    long corr = 0;
    for (int i = 0; i < BUFFER_SIZE - lag; i++)
      corr += (long)samples[i] * samples[i + lag];

    long r_lag = 0;
    for (int i = lag; i < BUFFER_SIZE; i++)
      r_lag += (long)samples[i] * samples[i];

    long denom = (r0 + r_lag) / 2;
    long merit = (denom > 0) ? (corr * 1000L) / denom : 0;

    if (merit > bestMerit) {
      bestMerit = merit;
      bestLag   = lag;
    }
  }

  if (bestLag < 0) return -1;

  int halfLag = bestLag / 2;
  if (halfLag >= minLag) {
    long halfCorr = 0;
    for (int i = 0; i < BUFFER_SIZE - halfLag; i++)
      halfCorr += (long)samples[i] * samples[i + halfLag];

    long halfR_lag = 0;
    for (int i = halfLag; i < BUFFER_SIZE; i++)
      halfR_lag += (long)samples[i] * samples[i];

    long halfDenom = (r0 + halfR_lag) / 2;
    long halfMerit = (halfDenom > 0) ? (halfCorr * 1000L) / halfDenom : 0;

    if (halfMerit >= (bestMerit * 85) / 100)
      bestLag = halfLag;
  }
  return (float)SAMPLE_RATE / bestLag;
}

// String identification
int identifyString(float freq) {
  int   bestIdx  = -1;
  float bestDiff = 1e9;
  for (int i = 0; i < NUM_STRINGS; i++) {
    float diff = abs(freq - STRINGS[i].freq);
    if (diff < STRINGS[i].freq * 0.35 && diff < bestDiff) {
      bestDiff = diff;
      bestIdx  = i;
    }
  }
  return bestIdx;
}

void loop() {
  collectSamples();
  float freq = detectPitch();

  if (freq < 0) {
    setLED(0, 0, 0);
    return;
  }

  int detected = identifyString(freq);
  if (detected < 0) {
    Serial.print("Unrecognised freq: ");
    Serial.println(freq);
    setLED(0, 0, 0);
    return;
  }
  if (detected != lastDetected) {
    resetAvg();
    lastDetected = detected;
    Serial.print("Detected string: ");
    Serial.println(STRINGS[detected].name);
  }

  pushFreq(freq);
  if (avgCount < 3) {
    setLED(0, 0, 0);
    return;
  }

  float smoothedFreq = avgFreq();
  float target       = STRINGS[detected].freq;
  float error        = smoothedFreq - target;

  Serial.print(STRINGS[detected].name);
  Serial.print("  Raw: ");    Serial.print(freq);
  Serial.print("  Avg: ");    Serial.print(smoothedFreq);
  Serial.print("  Error: ");  Serial.print(error);
  Serial.println(" Hz");

  if (abs(error) <= TUNE_TOLERANCE) {
    setLED(0, 255, 0);
    return;
  }

  if (error < 0) {
    setLED(255, 180, 0);   // flat = yellow
  } else {
    setLED(255, 60, 0);    // sharp = orange
  }

  int dir = (error < 0) ? -SHARP_DIR : SHARP_DIR;
  turnSteps(STEPS_PER_CORRECTION, dir);
}