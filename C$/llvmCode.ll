; ModuleID = 'module'
source_filename = "module"
target datalayout = "e-m:w-i64:64-f80:128-n8:16:32:64-S128"

%Class1 = type { i64* }

declare i8* @malloc(i64)

declare void @free(i8*)

define void @ClassInlineConstructors(%Class1*) {
Begin:
  %this = alloca %Class1*
  store %Class1* %0, %Class1** %this
  %1 = load %Class1*, %Class1** %this
  %2 = getelementptr %Class1, %Class1* %1, i64 0, i32 0
  %3 = call i8* @malloc(i64 8)
  %4 = bitcast i8* %3 to i64*
  store i64* %4, i64** %2
  %5 = load i64*, i64** %2
  ret void
}

define void @ClassDefaultCopyConstructor(%Class1*, %Class1*) {
Begin:
  %other = alloca %Class1*
  %this = alloca %Class1*
  store %Class1* %0, %Class1** %other
  store %Class1* %1, %Class1** %this
  %2 = load %Class1*, %Class1** %this
  %3 = getelementptr %Class1, %Class1* %2, i64 0, i32 0
  %4 = load %Class1*, %Class1** %other
  %5 = getelementptr %Class1, %Class1* %4, i64 0, i32 0
  %6 = load i64*, i64** %5
  %7 = ptrtoint i64* %6 to i64
  %8 = icmp eq i64 %7, 0
  br i1 %8, label %ifTrue, label %ifFalse

ifTrue:                                           ; preds = %Begin
  store i64* null, i64** %3
  br label %afterConditional

ifFalse:                                          ; preds = %Begin
  %9 = call i8* @malloc(i64 8)
  %10 = bitcast i8* %9 to i64*
  %11 = load i64, i64* %6
  store i64 %11, i64* %10
  store i64* %10, i64** %3
  br label %afterConditional

afterConditional:                                 ; preds = %ifFalse, %ifTrue
  %12 = load i64*, i64** %3
  ret void
}

define void @ClassDefaultCopyAssignment(%Class1*, %Class1*) {
Begin:
  %other = alloca %Class1*
  %this = alloca %Class1*
  store %Class1* %0, %Class1** %other
  store %Class1* %1, %Class1** %this
  %2 = load %Class1*, %Class1** %this
  %3 = getelementptr %Class1, %Class1* %2, i64 0, i32 0
  %4 = load %Class1*, %Class1** %other
  %5 = getelementptr %Class1, %Class1* %4, i64 0, i32 0
  %6 = load i64*, i64** %5
  %7 = ptrtoint i64* %6 to i64
  %8 = icmp eq i64 %7, 0
  br i1 %8, label %ifTrue, label %ifFalse

ifTrue:                                           ; preds = %Begin
  %9 = load i64*, i64** %3
  %10 = ptrtoint i64* %9 to i64
  %11 = icmp ne i64 %10, 0
  br i1 %11, label %ifTrue1, label %afterConditional

ifTrue1:                                          ; preds = %ifTrue
  %12 = bitcast i64* %9 to i8*
  call void @free(i8* %12)
  br label %afterConditional

afterConditional:                                 ; preds = %ifTrue, %ifTrue1
  store i64* null, i64** %3
  br label %afterConditional5

ifFalse:                                          ; preds = %Begin
  %13 = load i64*, i64** %3
  %14 = ptrtoint i64* %13 to i64
  %15 = icmp eq i64 %14, 0
  br i1 %15, label %ifTrue2, label %ifFalse3

ifTrue2:                                          ; preds = %ifFalse
  %16 = call i8* @malloc(i64 8)
  %17 = bitcast i8* %16 to i64*
  %18 = load i64, i64* %6
  store i64 %18, i64* %17
  store i64* %17, i64** %3
  br label %afterConditional4

ifFalse3:                                         ; preds = %ifFalse
  %19 = load i64, i64* %6
  store i64 %19, i64* %13
  br label %afterConditional4

afterConditional4:                                ; preds = %ifFalse3, %ifTrue2
  br label %afterConditional5

afterConditional5:                                ; preds = %afterConditional4, %afterConditional
  %20 = load i64*, i64** %3
  ret void
}

define void @ClassInlineDestructors(%Class1*) {
Begin:
  %this = alloca %Class1*
  store %Class1* %0, %Class1** %this
  %1 = load %Class1*, %Class1** %this
  %2 = getelementptr %Class1, %Class1* %1, i64 0, i32 0
  %3 = load i64*, i64** %2
  %4 = ptrtoint i64* %3 to i64
  %5 = icmp ne i64 %4, 0
  br i1 %5, label %ifTrue, label %afterConditional

ifTrue:                                           ; preds = %Begin
  %6 = bitcast i64* %3 to i8*
  call void @free(i8* %6)
  br label %afterConditional

afterConditional:                                 ; preds = %Begin, %ifTrue
  ret void
}

define i64 @main() {
Begin:
  %0 = alloca %Class1
  call void @ClassInlineConstructors(%Class1* %0)
  %1 = load %Class1, %Class1* %0
  call void @ClassInlineDestructors(%Class1* %0)
  ret i64 0
}
