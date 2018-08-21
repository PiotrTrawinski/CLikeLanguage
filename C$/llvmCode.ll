; ModuleID = 'module'
source_filename = "module"
target datalayout = "e-m:w-i64:64-f80:128-n8:16:32:64-S128"

%Point12 = type { i64, i64 }

declare i64 @putchar(i64)

declare i64 @printf(i8*)

define void @printInt(i64) {
Begin:
  %integer = alloca i64
  store i64 %0, i64* %integer
  %1 = load i64, i64* %integer
  %2 = icmp eq i64 %1, 0
  br i1 %2, label %if, label %afterIf

if:                                               ; preds = %Begin
  %3 = call i64 @putchar(i64 48)
  ret void

afterIf:                                          ; preds = %Begin
  %result = alloca [31 x i64]
  %isNegative = alloca i1
  %4 = load i64, i64* %integer
  %5 = icmp slt i64 %4, 0
  store i1 %5, i1* %isNegative
  %6 = load i1, i1* %isNegative
  %7 = load i1, i1* %isNegative
  br i1 %7, label %if1, label %afterIf2

if1:                                              ; preds = %afterIf
  %8 = load i64, i64* %integer
  %9 = sub i64 0, %8
  store i64 %9, i64* %integer
  %10 = load i64, i64* %integer
  br label %afterIf2

afterIf2:                                         ; preds = %afterIf, %if1
  %index = alloca i64
  store i64 31, i64* %index
  %11 = load i64, i64* %index
  br label %whileCondition

whileCondition:                                   ; preds = %while, %afterIf2
  %12 = load i64, i64* %integer
  %13 = icmp sgt i64 %12, 0
  br i1 %13, label %while, label %afterWhile

while:                                            ; preds = %whileCondition
  %14 = load i64, i64* %index
  %15 = sub i64 %14, 1
  store i64 %15, i64* %index
  %16 = load i64, i64* %index
  %17 = load i64, i64* %index
  %18 = getelementptr [31 x i64], [31 x i64]* %result, i64 0, i64 %17
  %19 = load i64, i64* %integer
  %20 = srem i64 %19, 10
  %21 = add i64 %20, 48
  store i64 %21, i64* %18
  %22 = load i64, i64* %18
  %23 = load i64, i64* %integer
  %24 = sdiv i64 %23, 10
  store i64 %24, i64* %integer
  %25 = load i64, i64* %integer
  br label %whileCondition

afterWhile:                                       ; preds = %whileCondition
  %26 = load i1, i1* %isNegative
  br i1 %26, label %if3, label %afterIf4

if3:                                              ; preds = %afterWhile
  %27 = load i64, i64* %index
  %28 = sub i64 %27, 1
  store i64 %28, i64* %index
  %29 = load i64, i64* %index
  %30 = load i64, i64* %index
  %31 = getelementptr [31 x i64], [31 x i64]* %result, i64 0, i64 %30
  store i64 45, i64* %31
  %32 = load i64, i64* %31
  br label %afterIf4

afterIf4:                                         ; preds = %afterWhile, %if3
  br label %forStart

forStart:                                         ; preds = %afterIf4
  %i = alloca i64
  %33 = load i64, i64* %index
  store i64 %33, i64* %i
  %34 = load i64, i64* %i
  br label %forCondition

forStep:                                          ; preds = %for
  %35 = load i64, i64* %i
  %36 = add i64 %35, 1
  store i64 %36, i64* %i
  %37 = load i64, i64* %i
  br label %forCondition

forCondition:                                     ; preds = %forStep, %forStart
  %38 = load i64, i64* %i
  %39 = icmp sle i64 %38, 30
  br i1 %39, label %for, label %afterFor

for:                                              ; preds = %forCondition
  %40 = load i64, i64* %i
  %41 = getelementptr [31 x i64], [31 x i64]* %result, i64 0, i64 %40
  %42 = load i64, i64* %41
  %43 = call i64 @putchar(i64 %42)
  br label %forStep

afterFor:                                         ; preds = %forCondition
  ret void
}

define void @printIntNewLine(i64) {
Begin:
  %integer = alloca i64
  store i64 %0, i64* %integer
  %1 = load i64, i64* %integer
  call void @printInt(i64 %1)
  %2 = call i64 @putchar(i64 10)
  ret void
}

define i64 @main() {
Begin:
  %point = alloca %Point12
  call void @0(i64 13, %Point12* %point)
  call void @printIntNewLine(i64 16)
  call void @printIntNewLine(i64 16)
  %0 = getelementptr %Point12, %Point12* %point, i64 0, i32 0
  %1 = load i64, i64* %0
  %2 = getelementptr %Point12, %Point12* %point, i64 0, i32 1
  %3 = load i64, i64* %2
  %4 = add i64 %1, %3
  ret i64 %4
}

define void @0(i64, %Point12*) {
Begin:
  %value = alloca i64
  store i64 %0, i64* %value
  %this = alloca %Point12*
  store %Point12* %1, %Point12** %this
  %2 = load %Point12*, %Point12** %this
  call void @1(%Point12* %2)
  %3 = load %Point12*, %Point12** %this
  %4 = getelementptr %Point12, %Point12* %3, i64 0, i32 1
  %5 = load i64, i64* %value
  store i64 %5, i64* %4
  %6 = load i64, i64* %4
  ret void
}

define void @1(%Point12*) {
Begin:
  %this = alloca %Point12*
  store %Point12* %0, %Point12** %this
  %1 = load %Point12*, %Point12** %this
  %2 = getelementptr %Point12, %Point12* %1, i64 0, i32 0
  store i64 13, i64* %2
  %3 = load i64, i64* %2
  ret void
}

define void @fun() {
Begin:
  ret void
}

define void @constructor(%Point12*) {
Begin:
  %this = alloca %Point12*
  store %Point12* %0, %Point12** %this
  %1 = load %Point12*, %Point12** %this
  call void @1(%Point12* %1)
  %2 = load %Point12*, %Point12** %this
  %3 = getelementptr %Point12, %Point12* %2, i64 0, i32 1
  store i64 0, i64* %3
  %4 = load i64, i64* %3
  ret void
}
