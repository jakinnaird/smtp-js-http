var apiKey = "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx";

function main(email) {

    var json = '{' + "\n"
        + '"message": "' + email.subject + '",' + "\n"
        + '"description": "' + email.body + '"' + "\n"
        + '}';

    var http = new WebRequest();
    http.header("Content-Type", "application/json");
    http.header("Authorization", "GenieKey " + apiKey);
    http.data(json);
    if (http.post("https://api.opsgenie.com/v2/alerts") != 0) {

        warn("error: " + http.error);
    }

    info(http.result);
}
