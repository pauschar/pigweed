// Copyright 2023 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

use proc_macro::TokenStream;
use proc_macro2::Ident;
use pw_format::macros::{
    generate, generate_printf, FormatAndArgs, FormatMacroGenerator, IntegerDisplayType,
    PrintfFormatMacroGenerator, Result,
};
use quote::quote;
use syn::{
    parse::{Parse, ParseStream},
    parse_macro_input, Expr, Token,
};

type TokenStream2 = proc_macro2::TokenStream;

// Generator for testing `generate()`.
struct TestGenerator {
    code_fragments: Vec<TokenStream2>,
}

impl TestGenerator {
    pub fn new() -> Self {
        Self {
            code_fragments: Vec::new(),
        }
    }
}

impl FormatMacroGenerator for TestGenerator {
    fn finalize(self) -> Result<TokenStream2> {
        let code_fragments = self.code_fragments;

        Ok(quote! {
          {
            use pw_format_test_macros_test::TestGeneratorOps;
            let mut ops = Vec::new();
            #(#code_fragments);*;
            ops.push(TestGeneratorOps::Finalize);
            ops
          }
        })
    }

    fn string_fragment(&mut self, string: &str) -> Result<()> {
        self.code_fragments.push(quote! {
            ops.push(TestGeneratorOps::StringFragment(#string.to_string()));
        });
        Ok(())
    }

    // This example ignores display type and width.
    fn integer_conversion(
        &mut self,
        display_type: IntegerDisplayType,
        type_width: u8, // in bits
        _expression: Expr,
    ) -> Result<()> {
        self.code_fragments.push(quote! {
            ops.push(TestGeneratorOps::IntegerConversion{
                display_type: #display_type,
                type_width: #type_width
            });
        });
        Ok(())
    }

    fn string_conversion(&mut self, expression: Expr) -> Result<()> {
        self.code_fragments.push(quote! {
            ops.push(TestGeneratorOps::StringConversion);
        });
        Ok(())
    }

    fn char_conversion(&mut self, expression: Expr) -> Result<()> {
        self.code_fragments.push(quote! {
            ops.push(TestGeneratorOps::CharConversion);
        });
        Ok(())
    }
}

#[proc_macro]
pub fn generator_test_macro(tokens: TokenStream) -> TokenStream {
    let format_and_args = parse_macro_input!(tokens as FormatAndArgs);
    let generator = TestGenerator::new();
    match generate(generator, format_and_args) {
        Ok(token_stream) => token_stream.into(),
        Err(e) => e.to_compile_error().into(),
    }
}

// Generator for testing `generate_printf()`.  Allows control over the return
// value of the conversion functions.
struct PrintfTestGenerator {
    format_string: Option<String>,
    code_fragments: Vec<TokenStream2>,
    integer_specifier_override: Option<String>,
    string_specifier_override: Option<String>,
    char_specifier_override: Option<String>,
}

impl PrintfTestGenerator {
    pub fn new() -> Self {
        Self {
            format_string: Some(String::new()),
            code_fragments: Vec::new(),
            integer_specifier_override: None,
            string_specifier_override: None,
            char_specifier_override: None,
        }
    }

    pub fn with_integer_specifier(mut self, specifier: &str) -> Self {
        self.integer_specifier_override = Some(specifier.to_string());
        self
    }

    pub fn with_string_specifier(mut self, specifier: &str) -> Self {
        self.string_specifier_override = Some(specifier.to_string());
        self
    }

    pub fn with_char_specifier(mut self, specifier: &str) -> Self {
        self.char_specifier_override = Some(specifier.to_string());
        self
    }
}

impl PrintfFormatMacroGenerator for PrintfTestGenerator {
    fn finalize(self, format_string: String) -> Result<TokenStream2> {
        // Create locally scoped alias so we can refer to them in `quote!()`.
        let code_fragments = self.code_fragments;

        Ok(quote! {
          {
            use pw_format_test_macros_test::PrintfTestGeneratorOps;
            let mut ops = Vec::new();
            #(#code_fragments);*;
            ops.push(PrintfTestGeneratorOps::Finalize);
            (#format_string, ops)
          }
        })
    }
    fn string_fragment(&mut self, string: &str) -> Result<()> {
        self.code_fragments.push(quote! {
            ops.push(PrintfTestGeneratorOps::StringFragment(#string.to_string()));
        });
        Ok(())
    }
    fn integer_conversion(&mut self, ty: Ident, _expression: Expr) -> Result<Option<String>> {
        let ty_str = format!("{ty}");
        self.code_fragments.push(quote! {
            ops.push(PrintfTestGeneratorOps::IntegerConversion{ ty: #ty_str.to_string() });
        });
        Ok(self.integer_specifier_override.clone())
    }

    fn string_conversion(&mut self, _expression: Expr) -> Result<Option<String>> {
        self.code_fragments.push(quote! {
            ops.push(PrintfTestGeneratorOps::StringConversion);
        });
        Ok(self.string_specifier_override.clone())
    }

    fn char_conversion(&mut self, _expression: Expr) -> Result<Option<String>> {
        self.code_fragments.push(quote! {
            ops.push(PrintfTestGeneratorOps::CharConversion);
        });
        Ok(self.char_specifier_override.clone())
    }
}

#[proc_macro]
pub fn printf_generator_test_macro(tokens: TokenStream) -> TokenStream {
    let format_and_args = parse_macro_input!(tokens as FormatAndArgs);
    let generator = PrintfTestGenerator::new();
    match generate_printf(generator, format_and_args) {
        Ok(token_stream) => token_stream.into(),
        Err(e) => e.to_compile_error().into(),
    }
}

// Causes the generator to substitute %d with %K.
#[proc_macro]
pub fn integer_sub_printf_generator_test_macro(tokens: TokenStream) -> TokenStream {
    let format_and_args = parse_macro_input!(tokens as FormatAndArgs);
    let generator = PrintfTestGenerator::new().with_integer_specifier("%K");
    match generate_printf(generator, format_and_args) {
        Ok(token_stream) => token_stream.into(),
        Err(e) => e.to_compile_error().into(),
    }
}

// Causes the generator to substitute %s with %K.
#[proc_macro]
pub fn string_sub_printf_generator_test_macro(tokens: TokenStream) -> TokenStream {
    let format_and_args = parse_macro_input!(tokens as FormatAndArgs);
    let generator = PrintfTestGenerator::new().with_string_specifier("%K");
    match generate_printf(generator, format_and_args) {
        Ok(token_stream) => token_stream.into(),
        Err(e) => e.to_compile_error().into(),
    }
}

// Causes the generator to substitute %c with %K.
#[proc_macro]
pub fn char_sub_printf_generator_test_macro(tokens: TokenStream) -> TokenStream {
    let format_and_args = parse_macro_input!(tokens as FormatAndArgs);
    let generator = PrintfTestGenerator::new().with_char_specifier("%K");
    match generate_printf(generator, format_and_args) {
        Ok(token_stream) => token_stream.into(),
        Err(e) => e.to_compile_error().into(),
    }
}
