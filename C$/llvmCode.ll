; ModuleID = 'module'
source_filename = "module"
target datalayout = "e-m:w-i64:64-f80:128-n8:16:32:64-S128"

define i64 @main() {
Begin:
  %x = alloca double
  store double 0.000000e+00, double* %x
  %0 = load double, double* %x
  br label %forStart

forStart:                                         ; preds = %Begin
  %i = alloca i64
  store i64 1, i64* %i
  %1 = load i64, i64* %i
  br label %forCondition

forStep:                                          ; preds = %for
  %2 = load i64, i64* %i
  %3 = add i64 %2, 1
  store i64 %3, i64* %i
  %4 = load i64, i64* %i
  br label %forCondition

forCondition:                                     ; preds = %forStep, %forStart
  %5 = load i64, i64* %i
  %6 = icmp sle i64 %5, 5
  br i1 %6, label %for, label %afterFor

for:                                              ; preds = %forCondition
  %7 = load double, double* %x
  %8 = load i64, i64* %i
  %9 = sitofp i64 %8 to double
  %10 = fadd double %7, %9
  store double %10, double* %x
  %11 = load double, double* %x
  br label %forStep

afterFor:                                         ; preds = %forCondition
  %12 = load double, double* %x
  %13 = fptosi double %12 to i64
  ret i64 %13
}

declare i64 @sin()
