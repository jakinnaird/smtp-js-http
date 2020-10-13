# smtp-js-http
Simple SMTP to HTTP translation with JavaScript processing

## Overview
This service allows SMTP traffic to be processed by a JavaScript program with the ability to call an HTTP endpoint.

## Building
Requires libsystemd-dev and libcurl-dev
> make

## Installing
> sudo make install

## Usage
The service acts like a basic SMTP server, that can be used as a delivery point for your applications. The "TO" email address is the discriminator which selects the JavaScript code to execute.

For example:
> Sending email to: main@opsgenie.js will attempt to load the JavaScript file named opsgenie.js from the scripts folder, and then call the function 'main' in that file.

You can have multiple functions within a single JavaScript file, and only the function named in the TO email address will be executed.

## JavaScript API

### API
Currently the following two classes are available in the JavaScript environment:

- email
    - Properties:
        - from [read-only, string] - the from email address
        - date [read-only, string] - the date provided by the SMTP client
        - subject [read-only, string] - the subject line of the email
        - body [read-only, string] - the body of the email

- WebRequest
    - Properties:
        - result [read-only, string] - the result of the called method
        - error [read-only, string] - error message resulting from the called method
    - Methods:
        - header(name, value) - set a header in the web request
        - data(postData) - sets the POST data to send
        - get(url) - performs a GET request on the provided URL
        - post(url) - performs a POST request on the provided URL

Global functions:

- info(msg) - writes an informational level log entry
- warn(msg) - writes a warning level log entry
- error(msg) - writes an error level log entry
- debug(msg) - writes a debug level log entry

### Function prototype
Any function to be called by the service needs to have the following prototype:

> function main(email) {}

The function name can be any valid function name, and must take an email object as it's single parameter.